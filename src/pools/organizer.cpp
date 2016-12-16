/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/pools/organizer.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <numeric>
#include <utility>
#include <thread>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/pools/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "organizer"

// Database access is limited to: push, pop, last-height, fork-difficulty,
// validator->populator:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

static inline size_t cores(const settings& settings)
{
    const auto configured = settings.cores;
    const auto hardware = std::max(std::thread::hardware_concurrency(), 1u);
    return configured == 0 ? hardware : std::min(configured, hardware);
}

static inline thread_priority priority(const settings& settings)
{
    return settings.priority ? thread_priority::high : thread_priority::normal;
}

organizer::organizer(threadpool& thread_pool,
    fast_chain& chain, block_pool& block_pool, const settings& settings)
  : fast_chain_(chain),
    stopped_(true),
    flush_reorganizations_(settings.flush_reorganizations),
    block_pool_(block_pool),
    priority_pool_(cores(settings), priority(settings)),
    priority_dispatch_(priority_pool_, NAME "_priority"),
    validator_(priority_pool_, fast_chain_, settings),
    subscriber_(std::make_shared<reorganize_subscriber>(thread_pool, NAME)),
    dispatch_(thread_pool, NAME "_dispatch")
{
}

// Properties.
//-----------------------------------------------------------------------------

bool organizer::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool organizer::start()
{
    stopped_ = false;
    subscriber_->start();

    // Don't begin flush lock if flushing on each reorganization.
    return flush_reorganizations_ || fast_chain_.begin_writes();
}

bool organizer::stop()
{
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, 0, {}, {});

    // Ensure that this call blocks until database writes are complete.
    // Ensure no reorganization is in process when the flush lock is cleared.
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(mutex_);

    // Ensure that a new validation will not begin after this stop. Otherwise
    // termination of the threadpool will corrupt the database.
    stopped_ = true;

    // Don't end flush lock if flushing on each reorganization.
    return flush_reorganizations_ || fast_chain_.end_writes();
    ///////////////////////////////////////////////////////////////////////////
}

// Organize sequence.
//-----------------------------------------------------------------------------

// This is called from block_chain::do_store(), a critical section.
void organizer::organize(block_const_ptr block,
    result_handler handler)
{
    code error_code;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section.
    // Use scope lock to guard the chain against concurrent organizations.
    // If a reorganization started after stop it will stop before writing.
    const auto lock = std::make_shared<scope_lock>(mutex_);

    if (stopped_)
    {
        handler(error::service_stopped);
        return;
    }

    // Checks that are independent of chain state.
    if ((error_code = validator_.check(block)))
    {
        handler(error_code);
        return;
    }

    const result_handler locked_handler =
        std::bind(&organizer::complete,
            this, _1, lock, handler);

    //*************************************************************************
    // CONSENSUS: This is the same check performed by satoshi, yet it will
    // produce a chain split in the case of a hash collision. This is because
    // it is not applied at the fork point, so some nodes will not see the
    // collision block and others will, depending on block order of arrival.
    // TODO: The hash check should start at the fork point. The duplicate check
    // is a conflated network denial of service protection mechanism and cannot
    // be allowed to reject blocks based on collisions not in the actual chain.
    //*************************************************************************
    // check database and orphan pool for duplicate block hash.
    if (fast_chain_.get_block_exists(block->hash()) ||
        !block_pool_.add(block))
    {
        locked_handler(error::duplicate_block);
        return;
    }

    // Find longest fork of blocks that connects the block to the blockchain.
    const auto fork = find_connected_fork(block);

    if (fork->empty())
    {
        // There is no link to the chain so the block is currently an orphan.
        locked_handler(error::orphan_block);
        return;
    }

    const auto first_height = safe_add(fork->height(), size_t(1));
    const auto maximum = fork->difficulty();
    uint256_t threshold;

    if (!fast_chain_.get_fork_difficulty(threshold, maximum, first_height))
    {
        locked_handler(error::operation_failed);
        return;
    }

    // Store required difficulty to overcome the main chain above fork point.
    fork->set_threshold(std::move(threshold));

    if (!fork->is_sufficient())
    {
        locked_handler(error::insufficient_work);
        return;
    }

    // Start the loop by verifying the first fork block.
    verify(fork, 0, locked_handler);
}

void organizer::complete(const code& ec, scope_lock::ptr lock,
    result_handler handler)
{
    lock.reset();
    // End Critical Section.
    ///////////////////////////////////////////////////////////////////////////

    // This is the end of the organize sequence.
    handler(ec);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// Verify the block at the given index in the fork.
void organizer::verify(fork::ptr fork, size_t index,
    result_handler handler)
{
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Preserve validation priority pool by returning on a network thread.
    const result_handler accept_handler = 
        dispatch_.bound_delegate(&organizer::handle_accept,
            this, _1, fork, index, handler);

    if (fork->is_verified(index))
    {
        // Validation already done, handle in accept.
        accept_handler(error::success);
        return;
    }

    // Protect the fork from the validator.
    auto const_fork = std::const_pointer_cast<blockchain::fork>(fork);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(const_fork, index, accept_handler);
}

void organizer::handle_accept(const code& ec, fork::ptr fork,
    size_t index, result_handler handler)
{
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec == error::service_stopped || ec == error::operation_failed)
    {
        // This is not a validation failure, so no pool removal.
        handler(ec);
        return;
    }

    // Preserve validation priority pool by returning on a network thread.
    // This also protects our stack from exhaustion due to recursion.
    const result_handler connect_handler = 
        dispatch_.bound_delegate(&organizer::handle_connect,
            this, _1, fork, index, handler);

    if (ec || fork->is_verified(index))
    {
        // Validation already done or failed, handle in connect.
        connect_handler(ec);
        return;
    }

    // Protect the fork from the validator.
    auto const_fork = std::const_pointer_cast<blockchain::fork>(fork);

    // Checks that include script validation.
    validator_.connect(const_fork, index, connect_handler);
}

// Call handler to stop, organized to continue.
void organizer::handle_connect(const code& ec, fork::ptr fork,
    size_t index, result_handler handler)
{
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec == error::service_stopped || ec == error::operation_failed)
    {
        // This is not a validation failure, so no pool removal.
        handler(ec);
        return;
    }

    if (ec)
    {
        // The index block failed to verify, remove it and descendants.
        block_pool_.remove(fork->pop(index, ec));

        // If we just cleared out the entire fork, return bad block's code.
        if (fork->empty())
        {
            handler(ec);
            return;
        }

        // Reverify that there is sufficient work in the fork to reorganize.
        if (!fork->is_sufficient())
        {
            handler(error::insufficient_work);
            return;
        }
    }
    else
    {
        // The index block is verified, ensure it is marked (may be already).
        fork->set_verified(index);
    }

    // Move to next block in the fork.
    const auto next = safe_increment(index);

    if (next < fork->size())
    {
        // Recurse: this *requires* thread change to prevent stack exhaustion.
        verify(fork, next, handler);
        return;
    }

    // If the loop is done (due to iteration or removal) attempt to reorg.
    organized(fork, handler);
}

// Attempt to reorganize the blockchain using the remaining valid fork.
void organizer::organized(fork::ptr fork, result_handler handler)
{
    BITCOIN_ASSERT(!fork->empty());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // The fork is valid and can now be used to notify subscribers.
    const auto block = fork->blocks()->front();
    block->validation.start_notify = asio::steady_clock::now();

    // Capture the outgoing blocks and forward to reorg handler.
    const auto out_blocks = std::make_shared<block_const_ptr_list>();

    // Protect the fork from the blockchain.
    auto const_fork = std::const_pointer_cast<blockchain::fork>(fork);

    const auto complete =
        std::bind(&organizer::handle_reorganized,
            this, _1, const_fork, out_blocks, handler);

    // Replace! Switch!
    //#########################################################################
    fast_chain_.reorganize(const_fork, out_blocks, flush_reorganizations_,
        priority_dispatch_, complete);
    //#########################################################################
}

void organizer::handle_reorganized(const code& ec,
    fork::const_ptr fork, block_const_ptr_list_ptr outgoing_blocks,
    result_handler handler)
{
    BITCOIN_ASSERT(!fork->blocks()->empty());

    if (ec)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing block to store, is now corrupted: "
            << ec.message();
        handler(ec);
        return;
    }

    // Remove before add so that we don't overflow the pool and lose blocks.
    block_pool_.remove(fork->blocks());
    block_pool_.add(outgoing_blocks);

    // Protect the outgoing blocks from subscribers.
    auto old_blocks = std::const_pointer_cast<const block_const_ptr_list>(
        outgoing_blocks);

    // TODO: we can notify before reorg for mining scenario.
    // v3 reorg block order is reverse of v2, fork.back() is the new top.
    notify_reorganize(fork->height(), fork->blocks(), old_blocks);

    // This is the end of the verify sub-sequence.
    handler(error::success);
}

// Subscription.
//-----------------------------------------------------------------------------

void organizer::subscribe_reorganize(reorganize_handler&& handler)
{
    subscriber_->subscribe(std::move(handler),
        error::service_stopped, 0, {}, {});
}

void organizer::notify_reorganize(size_t fork_height,
    block_const_ptr_list_const_ptr fork,
    block_const_ptr_list_const_ptr original)
{
    // Invoke is required here to prevent subscription parsing from creating a
    // unsurmountable backlog during catch-up sync.
    subscriber_->invoke(error::success, fork_height, fork, original);
}

// Utility.
//-----------------------------------------------------------------------------

// Once connected we can discard fork segments that fail validation at height.
fork::ptr organizer::find_connected_fork(block_const_ptr block)
{
    // Get the longest possible chain containing this new block.
    const auto fork = block_pool_.trace(block);

    size_t fork_height;

    // Get blockchain parent of the oldest fork block and save to fork.
    if (fast_chain_.get_height(fork_height, fork->hash()))
        fork->set_height(fork_height);
    else
        fork->clear();

    return fork;
}

} // namespace blockchain
} // namespace libbitcoin
