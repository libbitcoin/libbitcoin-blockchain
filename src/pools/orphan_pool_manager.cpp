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
#include <bitcoin/blockchain/pools/orphan_pool_manager.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <numeric>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/orphan_pool.hpp>
#include <bitcoin/blockchain/pools/orphan_pool_manager.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "orphan_pool_manager"

// Database access is limited to: push, pop, last-height, fork-difficulty,
// validator->populator:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

orphan_pool_manager::orphan_pool_manager(threadpool& thread_pool,
    fast_chain& chain, orphan_pool& orphan_pool, const settings& settings)
  : fast_chain_(chain),
    stopped_(true),
    orphan_pool_(orphan_pool),
    validator_(thread_pool, fast_chain_, settings),
    subscriber_(std::make_shared<reorganize_subscriber>(thread_pool, NAME)),
    dispatch_(thread_pool, NAME "_dispatch")
{
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

void orphan_pool_manager::start()
{
    stopped_ = false;
    subscriber_->start();
}

void orphan_pool_manager::stop()
{
    stopped_ = true;
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, 0, {}, {});
}

bool orphan_pool_manager::stopped() const
{
    return stopped_;
}

// Organize.
//-----------------------------------------------------------------------------

// This is called from block_chain::do_store(), a critical section.
void orphan_pool_manager::organize(block_const_ptr block,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Checks that are independent of chain state.
    const auto ec = validator_.check(block);

    if (ec)
    {
        handler(ec);
        return;
    }

    // This is a free-roaming consensus check.
    // Check database and orphan pool for duplicate block hash.
    if (fast_chain_.get_block_exists(block->hash()) ||
        !orphan_pool_.add(block))
    {
        handler(error::duplicate);
        return;
    }

    // Find longest fork of blocks that connects the block to the blockchain.
    const auto fork = find_connected_fork(block);

    // If there is no connection the original block is currently an orphan.
    if (fork->empty())
    {
        handler(error::orphan);
        return;
    }

    // Start the loop by verifying the first block.
    verify(fork, 0, handler);
}

// Verify the block at the given index in the fork.
void orphan_pool_manager::verify(fork::ptr fork, size_t index,
    result_handler handler)
{
    BITCOIN_ASSERT(!fork->empty());
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (fork->is_verified(index))
    {
        // This must be dispatched in order to prevent recursion.
        dispatch_.concurrent(&orphan_pool_manager::handle_connect,
            this, error::success, fork, index, handler);
        return;
    }

    // Protect the fork from the validator.
    auto const_fork = std::const_pointer_cast<blockchain::fork>(fork);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(const_fork, index,
        std::bind(&orphan_pool_manager::handle_accept,
            this, _1, fork, index, handler));
}

void orphan_pool_manager::handle_accept(const code& ec, fork::ptr fork,
    size_t index, result_handler handler)
{
    BITCOIN_ASSERT(!fork->empty());
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        // This is not a validation failure, so no pool removal.
        handler(ec);
        return;
    }

    // Protect the fork from the validator.
    auto const_fork = std::const_pointer_cast<blockchain::fork>(fork);

    // Checks that include script validation.
    validator_.connect(const_fork, index,
        std::bind(&orphan_pool_manager::handle_connect,
            this, _1, fork, index, handler));
}

// Call handler to stop, organized to continue.
void orphan_pool_manager::handle_connect(const code& ec, fork::ptr fork,
    size_t index, result_handler handler)
{
    BITCOIN_ASSERT(!fork->empty());
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        // The index block failed to verify, remove it and descendants.
        orphan_pool_.remove(fork->pop(index, ec));
    }
    else
    {
        // The index block is verified, ensure it is marked (may be already).
        fork->set_verified(index);
    }

    // If we just cleared out the entire fork, return the guilty block's code.
    if (fork->empty())
    {
        handler(ec);
        return;
    }

    const auto next = safe_increment(index);

    // Loop back this next block up the fork (requires thread break above).
    if (next < fork->size())
    {
        verify(fork, next, handler);
        return;
    }

    // If the loop is done (due to iteration or removal) attempt to reorg.
    organized(fork, handler);
}

// Attempt to reorganize the blockchain using the remaining valid fork.
void orphan_pool_manager::organized(fork::ptr fork, result_handler handler)
{
    BITCOIN_ASSERT(!fork->empty());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // This is the height of the first block of each fork after the fork.
    const auto base_height = safe_add(fork->height(), size_t(1));
    hash_number original_difficulty;

    // Summarize the difficulty of the original chain from base_height to top.
    if (!fast_chain_.get_fork_difficulty(original_difficulty, base_height))
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure getting difficulty from [" << base_height << "]";
        handler(error::operation_failed);
        return;
    }

    // Summarize difficulty of fork and reorganize only if exceeds original.
    if (fork->difficulty() <= original_difficulty)
    {
        log::debug(LOG_BLOCKCHAIN)
            << "Insufficient work to reorganize from [" << base_height << "]";
        handler(error::insufficient_work);
        return;
    }

    // Replace! Switch!
    list original;

    // Remove the original chain blocks from the store.
    if (!fast_chain_.pop_from(original, base_height))
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure reorganizing from [" << base_height << "]";
        handler(error::operation_failed);
        return;
    }

    if (!original.empty())
    {
        log::info(LOG_BLOCKCHAIN)
            << "Reorganizing from block " << base_height << " to "
            << safe_add(base_height, original.size()) << "]";
    }

    auto height = fork->height();

    for (size_t index = 0; index < fork->size(); ++index)
    {
        const auto block = fork->block_at(index);

        // Remove the fork block from the orphan pool.
        orphan_pool_.remove(block);

        ///////////////////////////////////////////////////////////////////////
        // TODO: defer store write lock until the block is actually written,
        // and instead place a lock on the pool manager's organize call.
        ///////////////////////////////////////////////////////////////////////
        // Add the fork block to the store (logs failures).
        if (!fast_chain_.push(block, safe_increment(height)))
        {
            handler(error::operation_failed);
            return;
        }
    }

    height = fork->height();

    for (const auto block: original)
    {
        // Original blocks remain valid at their original heights.
        block->validation.height = safe_increment(height);
        block->validation.result = error::success;

        // Add the original block to the orphan pool.
        orphan_pool_.add(block);
    }

    // v3 reorg block order is reverse of v2, fork.back() is the new top.
    notify_reorganize(fork->height(), fork->blocks(), original);
    handler(error::success);
}

// Subscription.
//-----------------------------------------------------------------------------

void orphan_pool_manager::subscribe_reorganize(reorganize_handler handler)
{
    subscriber_->subscribe(handler, error::service_stopped, 0, {}, {});
}

void orphan_pool_manager::notify_reorganize(size_t fork_height,
    const list& fork, const list& original)
{
    subscriber_->relay(error::success, fork_height, fork, original);
}

// Utility.
//-----------------------------------------------------------------------------

// Once connected we can discard fork segments that fail validation at height.
fork::ptr orphan_pool_manager::find_connected_fork(block_const_ptr block)
{
    // Get the longest possible chain containing this new block.
    const auto fork = orphan_pool_.trace(block);

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
