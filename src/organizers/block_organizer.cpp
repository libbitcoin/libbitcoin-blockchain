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
#include <bitcoin/blockchain/organizers/block_organizer.hpp>

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "block_organizer"

block_organizer::block_organizer(prioritized_mutex& mutex,
    dispatcher& dispatch, threadpool& thread_pool, fast_chain& chain,
    const settings& settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    dispatch_(dispatch),
    validator_(dispatch, chain, settings),
    subscriber_(std::make_shared<reorganize_subscriber>(thread_pool, NAME))
{
}

// Properties.
//-----------------------------------------------------------------------------

bool block_organizer::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool block_organizer::start()
{
    stopped_ = false;
    subscriber_->start();
    validator_.start();
    return true;
}

bool block_organizer::stop()
{
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, 0, {}, {});
    stopped_ = true;
    return true;
}

// Organize sequence.
//-----------------------------------------------------------------------------

void block_organizer::organize(block_const_ptr block, result_handler handler)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_high_priority();

    // Reset the reusable promise.
    resume_ = std::promise<code>();

    const result_handler complete =
        std::bind(&block_organizer::signal_completion,
            this, _1);

    const auto check_handler =
        std::bind(&block_organizer::handle_check,
            this, _1, block, complete);

    // Checks that are independent of chain state.
    validator_.check(block, check_handler);

    // Wait on completion signal.
    // This is necessary in order to continue on a non-priority thread.
    // If we do not wait on the original thread there may be none left.
    auto ec = resume_.get_future().get();

    mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////

    // Invoke caller handler outside of critical section.
    handler(ec);
}

// private
void block_organizer::signal_completion(const code& ec)
{
    // This must be protected so that it is properly cleared.
    // Signal completion, which results in original handler invoke with code.
    resume_.set_value(ec);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// private
void block_organizer::handle_check(const code& ec, block_const_ptr block,
    result_handler handler)
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

    const auto accept_handler =
        std::bind(&block_organizer::handle_accept,
            this, _1, block, handler);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(block, accept_handler);
}

// private
void block_organizer::handle_accept(const code& ec, block_const_ptr block,
    result_handler handler)
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

    const auto connect_handler =
        std::bind(&block_organizer::handle_connect,
            this, _1, block, handler);

    // Checks that include script validation.
    validator_.connect(block, connect_handler);
}

// private
void block_organizer::handle_connect(const code& ec, block_const_ptr block,
    result_handler handler)
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

    block->validation.start_notify = asio::steady_clock::now();

    /// TODO: verify set in populate.
    ////block->header().validation.height = ...;
    block->header().validation.error = error::success;

    // TODO: create a simulated validation path that does not block others.
    if (block->header().validation.simulate)
    {
        handler(error::success);
        return;
    }

    // Get the outgoing blocks to forward to reorg handler.
    const auto outgoing = std::make_shared<block_const_ptr_list>();

    const auto reorganized_handler =
        std::bind(&block_organizer::handle_reorganized,
            this, _1, block, outgoing, handler);

    // Update block state to valid.
    //#########################################################################
    ////fast_chain_.set_valid(block);
    //#########################################################################
}

// private
// Outgoing blocks must have median_time_past set.
void block_organizer::handle_reorganized(const code& ec,
    block_const_ptr_list_const_ptr incoming,
    block_const_ptr_list_ptr outgoing, result_handler handler)
{
    if (ec)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing block to store, is now corrupted: "
            << ec.message();
        handler(ec);
        return;
    }

    ////// v3 reorg block order is reverse of v2, branch.back() is the new top.
    ////notify(branch->height(), branch->blocks(), outgoing);

    handler(error::success);
}

// Subscription.
//-----------------------------------------------------------------------------

// private
void block_organizer::notify(size_t fork_height,
    block_const_ptr_list_const_ptr incoming,
    block_const_ptr_list_const_ptr outgoing)
{
    // This invokes handlers within the criticial section (deadlock risk).
    subscriber_->invoke(error::success, fork_height, incoming, outgoing);
}

void block_organizer::subscribe(reorganize_handler&& handler)
{
    subscriber_->subscribe(std::move(handler),
        error::service_stopped, 0, {}, {});
}

void block_organizer::unsubscribe()
{
    subscriber_->relay(error::success, 0, {}, {});
}

} // namespace blockchain
} // namespace libbitcoin
