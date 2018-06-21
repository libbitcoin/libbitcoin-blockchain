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
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::database;
using namespace std::placeholders;

#define NAME "transaction_organizer"

// TODO: create priority pool at blockchain level and use in both organizers. 
transaction_organizer::transaction_organizer(prioritized_mutex& mutex,
    dispatcher& dispatch, threadpool& thread_pool, fast_chain& chain,
    const settings& settings, const bc::settings& bitcoin_settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    settings_(settings),
    dispatch_(dispatch),
    transaction_pool_(settings, bitcoin_settings),
    validator_(dispatch, fast_chain_, settings),
    subscriber_(std::make_shared<transaction_subscriber>(thread_pool, NAME))
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
    subscriber_->start();
    validator_.start();
    return true;
}

bool transaction_organizer::stop()
{
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, {});
    stopped_ = true;
    return true;
}

// Organize sequence.
//-----------------------------------------------------------------------------

// This is called from block_chain::organize.
void transaction_organizer::organize(transaction_const_ptr tx,
    result_handler handler)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_low_priority();

    // Reset the reusable promise.
    resume_ = std::promise<code>();

    const result_handler complete =
        std::bind(&transaction_organizer::signal_completion,
            this, _1);

    const auto check_handler =
        std::bind(&transaction_organizer::handle_check,
            this, _1, tx, complete);

    // Checks that are independent of chain state.
    validator_.check(tx, check_handler);

    // Wait on completion signal.
    // This is necessary in order to continue on a non-priority thread.
    // If we do not wait on the original thread there may be none left.
    auto ec = resume_.get_future().get();

    mutex_.unlock_low_priority();
    ///////////////////////////////////////////////////////////////////////////

    // Invoke caller handler outside of critical section.
    handler(ec);
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
//*****************************************************************************
// CONSENSUS:
// It is OK for us to restrict *pool* transactions to those that do not collide
// with any in the chain (as well as any in the pool) as collision will result
// in monetary destruction and we don't want to facilitate it. We must allow
// collisions in *block* validation if that is configured as otherwise will not
// follow the chain when a collision is mined.
//*****************************************************************************
void transaction_organizer::handle_check(const code& ec,
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

    if (transaction_pool_.exists(tx))
    {
        // The tx is already memory pooled (nothing to do).
        handler(error::duplicate_transaction);
        return;
    }

    const auto accept_handler =
        std::bind(&transaction_organizer::handle_accept,
            this, _1, tx, handler);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(tx, accept_handler);
}

// private
void transaction_organizer::handle_accept(const code& ec,
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

    if (tx->fees() < price(tx))
    {
        handler(error::insufficient_fee);
        return;
    }

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

    // TODO: create a simulated validation path that does not block others.
    if (tx->metadata.simulate)
    {
        handler(error::success);
        return;
    }

    //#########################################################################
    const auto result = fast_chain_.push(tx);
    //#########################################################################

    if (!result)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing transaction to store, is now corrupted: ";
        handler(error::operation_failed);
        return;
    }

    // This gets picked up by node tx-out protocol for announcement to peers.
    notify(tx);
    handler(error::success);
}

// Subscription.
//-----------------------------------------------------------------------------

// private
void transaction_organizer::notify(transaction_const_ptr tx)
{
    // This invokes handlers within the criticial section (deadlock risk).
    subscriber_->invoke(error::success, tx);
}

void transaction_organizer::subscribe(transaction_handler&& handler)
{
    subscriber_->subscribe(std::move(handler), error::service_stopped, {});
}

void transaction_organizer::unsubscribe()
{
    subscriber_->relay(error::success, {});
}

// Queries.
//-----------------------------------------------------------------------------

void transaction_organizer::fetch_template(
    merkle_block_fetch_handler handler) const
{
    transaction_pool_.fetch_template(handler);
}

void transaction_organizer::fetch_mempool(size_t maximum,
    inventory_fetch_handler handler) const
{
    transaction_pool_.fetch_mempool(maximum, handler);
}

void transaction_organizer::filter(get_data_ptr message) const
{
    transaction_pool_.filter(message);
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
