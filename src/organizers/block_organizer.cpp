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
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "block_organizer"

// Database access is limited to: push, pop, last-height, branch-work,
// validator->populator:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

block_organizer::block_organizer(prioritized_mutex& mutex, dispatcher& dispatch,
    threadpool& thread_pool, fast_chain& chain, const settings& settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    dispatch_(dispatch),
    block_pool_(settings.reorganization_limit),
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

// This is called from block_chain::organize.
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

    // Verify the last branch block (all others are verified).
    // Get the path through the block forest to the new block.
    const auto branch = block_pool_.get_path(block);

    //*************************************************************************
    // CONSENSUS: This is the same check performed by satoshi, yet it will
    // produce a chain split in the case of a hash collision. This is because
    // it is not applied at the branch point, so some nodes will not see the
    // collision block and others will, depending on block order of arrival.
    //*************************************************************************
    if (branch->empty() || fast_chain_.get_block_exists(block->hash()))
    {
        handler(error::duplicate_block);
        return;
    }

    if (!set_branch_height(branch))
    {
        handler(error::orphan_block);
        return;
    }

    const auto accept_handler =
        std::bind(&block_organizer::handle_accept,
            this, _1, branch, handler);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(branch, accept_handler);
}

// private
void block_organizer::handle_accept(const code& ec, branch::ptr branch,
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
            this, _1, branch, handler);

    // Checks that include script validation.
    validator_.connect(branch, connect_handler);
}

// private
void block_organizer::handle_connect(const code& ec, branch::ptr branch,
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

    // The top block is valid even if the branch has insufficient work.
    // We aren't summing the chain work computation cost, but this is trivial
    // unless there is a reorganization as it just gets the top block height.
    const auto top = branch->top();
    top->header().validation.height = branch->top_height();
    top->validation.error = error::success;
    top->validation.start_notify = asio::steady_clock::now();

    const auto first_height = branch->height() + 1u;
    const auto maximum = branch->work();
    uint256_t threshold;

    // The chain query will stop if it reaches the maximum.
    if (!fast_chain_.get_branch_work(threshold, maximum, first_height))
    {
        handler(error::operation_failed);
        return;
    }

    // TODO: consider relay of pooled blocks by modifying subscriber semantics.
    if (branch->work() <= threshold)
    {
        if (!top->validation.simulate)
            block_pool_.add(branch->top());

        handler(error::insufficient_work);
        return;
    }

    // TODO: create a simulated validation path that does not block others.
    if (top->validation.simulate)
    {
        handler(error::success);
        return;
    }

    // Get the outgoing blocks to forward to reorg handler.
    const auto out_blocks = std::make_shared<block_const_ptr_list>();

    const auto reorganized_handler =
        std::bind(&block_organizer::handle_reorganized,
            this, _1, branch, out_blocks, handler);

    // Replace! Switch!
    //#########################################################################
    fast_chain_.reorganize(branch->fork_point(), branch->blocks(), out_blocks,
        dispatch_, reorganized_handler);
    //#########################################################################
}

// private
void block_organizer::handle_reorganized(const code& ec,
    branch::const_ptr branch, block_const_ptr_list_ptr outgoing,
    result_handler handler)
{
    if (ec)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing block to store, is now corrupted: "
            << ec.message();
        handler(ec);
        return;
    }

    block_pool_.remove(branch->blocks());
    block_pool_.prune(branch->top_height());
    block_pool_.add(outgoing);

    // v3 reorg block order is reverse of v2, branch.back() is the new top.
    notify(branch->height(), branch->blocks(), outgoing);

    handler(error::success);
}

// Subscription.
//-----------------------------------------------------------------------------

// private
void block_organizer::notify(size_t branch_height,
    block_const_ptr_list_const_ptr branch,
    block_const_ptr_list_const_ptr original)
{
    // This invokes handlers within the criticial section (deadlock risk).
    subscriber_->invoke(error::success, branch_height, branch, original);
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

// Queries.
//-----------------------------------------------------------------------------

void block_organizer::filter(get_data_ptr message) const
{
    block_pool_.filter(message);
}

// Utility.
//-----------------------------------------------------------------------------

// TODO: store this in the block pool and avoid this query.
bool block_organizer::set_branch_height(branch::ptr branch)
{
    size_t height;

    // Get blockchain parent of the oldest branch block.
    if (!fast_chain_.get_height(height, branch->hash()))
        return false;

    branch->set_height(height);
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
