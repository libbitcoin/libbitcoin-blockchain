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
#include <bitcoin/blockchain/pools/transaction_organizer.hpp>

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace std::placeholders;

#define NAME "transaction_organizer"

// TODO: create priority pool at blockchain level and use in both organizers. 
transaction_organizer::transaction_organizer(prioritized_mutex& mutex,
    dispatcher& dispatch, threadpool& thread_pool, fast_chain& chain,
    const settings& settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    minimum_byte_fee_(settings.minimum_byte_fee_satoshis),
    dispatch_(dispatch),
    transaction_pool_(settings),
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

    // The stop check must be guarded.
    if (stopped())
    {
        mutex_.unlock_low_priority();
        //---------------------------------------------------------------------
        handler(error::service_stopped);
        return;
    }

    // Checks that are independent of chain state.
    auto ec = validator_.check(tx);

    if (ec)
    {
        mutex_.unlock_low_priority();
        //---------------------------------------------------------------------
        handler(ec);
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
    ec = resume_.get_future().get();

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

    if (tx->fees() < minimum_byte_fee_ * tx->serialized_size(true))
    {
        handler(error::insufficient_fee);
        return;
    }

    const auto connect_handler =
        std::bind(&transaction_organizer::handle_connect,
            this, _1, tx, handler);

    // Checks that include script validation.
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

    // TODO: create a simulated validation path that does not lock others.
    if (tx->validation.simulate)
    {
        handler(error::success);
        return;
    }

    const auto pushed_handler =
        std::bind(&transaction_organizer::handle_pushed,
            this, _1, tx, handler);

    //#########################################################################
    fast_chain_.push(tx, dispatch_, pushed_handler);
    //#########################################################################
}

// private
void transaction_organizer::handle_pushed(const code& ec,
    transaction_const_ptr tx, result_handler handler)
{
    // The store verifies this as a safeguard, but should have caught earlier.
    if (ec == error::unspent_duplicate)
    {
        handler(ec);
        return;
    }

    if (ec)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing transaction to store, is now corrupted: "
            << ec.message();
        handler(ec);
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
    // Using relay can create huge backlog, but careful of criticial section.
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

// Utility.
//-----------------------------------------------------------------------------

} // namespace blockchain
} // namespace libbitcoin
