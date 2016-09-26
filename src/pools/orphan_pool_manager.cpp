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
#include <bitcoin/blockchain/interface/simple_chain.hpp>
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

orphan_pool_manager::orphan_pool_manager(threadpool& thread_pool,
    simple_chain& chain, orphan_pool& pool, const settings& settings)
  : chain_(chain),
    validator_(thread_pool, settings.use_testnet_rules,
        settings.use_libconsensus, checkpoints_, chain_),
    testnet_rules_(settings.use_testnet_rules),
    checkpoints_(checkpoint::sort(settings.checkpoints)),
    subscriber_(std::make_shared<reorganize_subscriber>(thread_pool, NAME)),
    stopped_(true),
    pool_(pool)
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
    code ec;
    fork::ptr fork;

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Checks independent of the chain.
    if ((ec = validator_.check(block)))
    {
        handler(ec);
        return;
    }

    // Check database and orphan pool for duplicate block hash.
    if (chain_.get_exists(block->hash()) || !pool_.add(block))
    {
        handler(error::duplicate);
        return;
    }

    // Find longest fork of blocks that connects the block to the blockchain.
    // If there is no connection the original block is currently an orphan.
    if ((fork = find_connected_fork(block))->empty())
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
    code ec;
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Call this to continue the loop or handler to stop.
    const result_handler next_block =
        std::bind(&orphan_pool_manager::handle_verify,
            this, _1, fork, index, handler);

    // Bypass validation if previously validated for this height.
    if (fork->is_verified(index))
    {
        next_block(error::success);
        return;
    }

    const auto block = fork->block_at(index);
    const auto height = fork->height_at(index);

    // Configure chain state for accept/connect checks.
    if ((ec = validator_.reset(height)))
    {
        handler(ec);
        return;
    }

    // Populate the previous outputs of the block.
    if ((ec = validator_.populate(block)))
    {
        handler(ec);
        return;
    }

    // Checks that are dependent on chain state and prevouts.
    if ((ec = validator_.accept(block)))
    {
        handler(ec);
        return;
    }

    // Checks dependent on chain state, prevouts and perform script validation.
    validator_.connect(block, next_block);
}

// Call handler to stop, organized to coninue.
void orphan_pool_manager::handle_verify(const code& ec, fork::ptr fork,
    size_t index, result_handler handler)
{
    BITCOIN_ASSERT(!fork->empty());
    BITCOIN_ASSERT(index < fork->size());

    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // The index block failed to verify, remove it and descendants, or the
    // index block is verified, ensure it is marked (may be already).
    if (ec)
        pool_.remove(fork->pop(index, ec));
    else
        fork->set_verified(index);

    // If we just cleared out the entire fork, return the guilty block's code.
    if (fork->empty())
    {
        handler(ec);
        return;
    }

    const auto next = safe_increment(index);

    // If the loop is done (due to iteration or removal) attempt to reorg.
    // Otherwise continue the verify loop with the next block in the fork.
    if (next >= fork->size())
    {
        organized(fork, handler);
        return;
    }

    // This recursion ties up the stack until the end of verify.
    verify(fork, next, handler);
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

    // This is the height of the first block of each branch after the fork.
    const auto base_height = safe_add(fork->height(), size_t(1));
    hash_number original_difficulty;

    // Summarize the difficulty of the original chain from base_height to top.
    if (!chain_.get_difficulty(original_difficulty, base_height))
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
    if (!chain_.pop_from(original, base_height))
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
        pool_.remove(block);

        // Add the fork block to the store (logs failures).
        if (!chain_.push(block, safe_increment(height)))
        {
            handler(error::operation_failed);
            return;
        }
    }

    height = fork->height();

    for (const auto block: original)
    {
        // Original blocks remain valid at their original heights.
        block->metadata.validation_height = safe_increment(height);
        block->metadata.validation_result = error::success;

        // Add the original block to the orphan pool.
        pool_.add(block);
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
    const auto fork = pool_.trace(block);

    size_t fork_height;

    // Get blockchain parent of the oldest fork block and save to fork.
    if (chain_.get_height(fork_height, fork->hash()))
        fork->set_height(fork_height);
    else
        fork->clear();

    return fork;
}

} // namespace blockchain
} // namespace libbitcoin
