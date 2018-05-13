/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/organizers/transaction_organizer.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::database;
using namespace std::placeholders;

#define NAME "transaction_organizer"

transaction_organizer::transaction_organizer(prioritized_mutex& mutex,
    dispatcher& priority_dispatch, threadpool&, fast_chain& chain,
    transaction_pool& pool, const settings& settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    settings_(settings),
    pool_(pool),
    validator_(priority_dispatch, fast_chain_, settings)
{
}

// Properties.
//-----------------------------------------------------------------------------

bool transaction_organizer::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool transaction_organizer::start()
{
    stopped_ = false;
    validator_.start();
    return true;
}

bool transaction_organizer::stop()
{
    validator_.stop();
    stopped_ = true;
    return true;
}

// Organize sequence.
//-----------------------------------------------------------------------------
// This runs in single thread normal priority except for validation fan-outs.
// Therefore fan-outs may use all threads in the priority threadpool.

// This is called from block_chain::organize.
void transaction_organizer::organize(transaction_const_ptr tx,
    result_handler handler)
{
    code error_code;

    // Checks that are independent of chain state.
    if ((error_code = validator_.check(tx)))
    {
        handler(error_code);
        return;
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_low_priority();

    // The pool is safe for filtering only, so protect by critical section.
    // This locates only unconfirmed transactions discovered since startup.
    const auto exists = pool_.exists(tx);

    // See symmetry with header memory pool.
    // The tx is already memory pooled (nothing to do).
    if (exists)
    {
        mutex_.unlock_low_priority();
        //---------------------------------------------------------------------
        handler(error::duplicate_transaction);
        return;
    }

    // Reset the reusable promise.
    resume_ = std::promise<code>();

    const result_handler complete =
        std::bind(&transaction_organizer::signal_completion,
            this, _1);

    const auto accept_handler =
        std::bind(&transaction_organizer::handle_accept,
            this, _1, tx, complete);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(tx, accept_handler);

    // Wait on completion signal.
    // This is necessary in order to continue on a non-priority thread.
    // If we do not wait on the original thread there may be none left.
    error_code = resume_.get_future().get();

    mutex_.unlock_low_priority();
    ///////////////////////////////////////////////////////////////////////////

    // Invoke caller handler outside of critical section.
    handler(error_code);
}

// private
void transaction_organizer::signal_completion(const code& ec)
{
    // This must be protected so that it is properly cleared.
    // Signal completion, which results in original handler invoke with code.
    resume_.set_value(ec);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// private
void transaction_organizer::handle_accept(const code& ec,
    transaction_const_ptr tx, result_handler handler)
{
    // The tx may exist in the store in any state except confirmed or verified.
    // Either state implies that the tx exists and is valid for its context.

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        handler(ec);
        return;
    }

    // Policy.
    if (tx->fees() < price(tx))
    {
        handler(error::insufficient_fee);
        return;
    }

    // Policy.
    if (tx->is_dusty(settings_.minimum_output_satoshis))
    {
        handler(error::dusty_transaction);
        return;
    }

    const auto connect_handler =
        std::bind(&transaction_organizer::handle_connect,
            this, _1, tx, handler);

    // Checks that include script metadata.
    validator_.connect(tx, connect_handler);
}

// private
void transaction_organizer::handle_connect(const code& ec,
    transaction_const_ptr tx, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        handler(ec);
        return;
    }

    // TODO: add to pool_.
    //#########################################################################
    const auto error_code = fast_chain_.store(tx);
    //#########################################################################

    if (error_code)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing transaction to store, is now corrupted: ";
        handler(error_code);
        return;
    }

    handler(error_code);
}

// Utility.
//-----------------------------------------------------------------------------

uint64_t transaction_organizer::price(transaction_const_ptr tx) const
{
    const auto byte_fee = settings_.byte_fee_satoshis;
    const auto sigop_fee = settings_.sigop_fee_satoshis;

    // Guard against summing signed values by testing independently.
    if (byte_fee == 0.0f && sigop_fee == 0.0f)
        return 0;

    // TODO: this is a second pass on size and sigops, implement cache.
    // This at least prevents uncached calls when zero fee is configured.
    auto byte = byte_fee > 0 ? byte_fee * tx->serialized_size(true) : 0;
    auto sigop = sigop_fee > 0 ? sigop_fee * tx->signature_operations() : 0;

    // Require at least one satoshi per tx if there are any fees configured.
    return std::max(uint64_t(1), static_cast<uint64_t>(byte + sigop));
}

} // namespace blockchain
} // namespace libbitcoin
