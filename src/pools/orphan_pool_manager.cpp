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
#include <future>
#include <memory>
#include <numeric>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/simple_chain.hpp>
#include <bitcoin/blockchain/pools/orphan_pool.hpp>
#include <bitcoin/blockchain/pools/orphan_pool_manager.hpp>
#include <bitcoin/blockchain/settings.hpp>
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

// Not thread safe.
// This is called from block_chain::do_store(), a critical section.
code orphan_pool_manager::organize(block_const_ptr block)
{
    if (chain_.get_exists(block->hash()) || !pool_.add(block))
        return error::duplicate;

    // Get the chain that this block connects to (but not connected to it).
    auto fork = pool_.trace(block);

    // The fork must always include at least the new block.
    BITCOIN_ASSERT(!fork.empty());

    uint64_t fork_height;
    const auto& previous_block_hash = fork.front()->header.previous_block_hash;

    // The blockchain parent of the oldest fork block is at the fork height.
    if (!chain_.get_height(fork_height, previous_block_hash))
    {
        // Indicate that this block's chain does not connect to the blockchain,
        // as opposed to having insufficient work, etc. Ask peer for missing.
        set_height(block);
        set_result(block, error::orphan);
        return error::success;
    }

    // Validate the fork blocks and purge any invalid trailing segment.
    prune(fork, fork_height);

    // Attempt to reorganize the blockchain using the remaining valid fork.
    return reorganize(fork, fork_height);
}

// Verify the fork and remove any invalid trailing segment.
void orphan_pool_manager::prune(list& fork, size_t fork_height)
{
    code ec(error::success);

    for (size_t index = 0; !ec && index < fork.size(); ++index)
        if ((ec = verify(fork[index], to_height(fork_height, index))))
            purge(fork, index, ec);
}

// Validate a block for a given height.
code orphan_pool_manager::verify(block_const_ptr block, size_t height)
{
    if (stopped())
        return error::service_stopped;

    // Bypass validation if the block has previously validated for this height.
    if (validated(block, height))
        return error::success;

    code ec;

    // Checks that are independent of the chain.
    if ((ec = validator_.check(block)))
        return ec;

    if ((ec = validator_.reset(height)))
        return ec;

    // Checks that are dependent on chain state.
    if ((ec = validator_.accept(block)))
        return ec;

    std::promise<code> promise;

    const result_handler join_handler =
        std::bind(&orphan_pool_manager::handle_connect,
            this, _1, block, height, std::ref(promise));

    // Checks that are dependent on chain state and include script validation.
    validator_.connect(block, join_handler);

    // Block until the promise is signalled, returns the value passed.
    return promise.get_future().get();
}

// Invoked on thread join following completion of input validation.
void orphan_pool_manager::handle_connect(const code& ec, block_const_ptr block,
    size_t height, std::promise<code>& complete)
{
    set_height(block, height);
    set_result(block, error::success);

    // Singnal the main thread (verify) to continue.
    complete.set_value(ec);
}

// Purge connectable-but-invalid-at-connect-height blocks from the orphan pool.
void orphan_pool_manager::purge(list& fork, size_t start_index,
    const code& reason)
{
    BITCOIN_ASSERT(start_index < fork.size());
    const auto start = fork.begin() + start_index;

    for (auto it = start; it != fork.end(); ++it)
    {
        // The first block should already be configured with ec/height.
        // Indicate to handler/subscribers that the block is purged.
        set_result(*it, it == start ? reason : error::previous_block_invalid);

        // Remove the (invalid) fork block from the orphan pool.
        pool_.remove(*it);
    }

    fork.erase(start, fork.end());
}

// Replace! Switch!
code orphan_pool_manager::reorganize(list& fork, size_t fork_height)
{
    // These may have been no valid blocks in the trace.
    if (fork.empty())
        return error::success;

    // This is the height of the first block of each branch after the fork.
    const auto base_height = fork_height + 1;
    hash_number original_difficulty;

    // Summarize the difficulty of the original chain from base_height to top.
    if (!chain_.get_difficulty(original_difficulty, base_height))
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure getting difficulty from [" << base_height << "]";
        return error::operation_failed;
    }

    // Summarize difficulty of fork and reorganize only if exceeds original.
    if (fork_difficulty(fork) <= original_difficulty)
    {
        log::debug(LOG_BLOCKCHAIN)
            << "Insufficient work to reorganize from [" << base_height << "]";
        return error::success;
    }

    list original;

    // Remove the original chain blocks from the store.
    if (!chain_.pop_from(original, base_height))
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure reorganizing from [" << base_height << "]";
        return error::operation_failed;
    }

    if (!original.empty())
    {
        log::info(LOG_BLOCKCHAIN)
            << "Reorganizing from block " << base_height << " to "
            << base_height + original.size() << "]";
    }

    auto height = base_height;

    for (const auto block: fork)
    {
        // Remove the fork block from the orphan pool.
        pool_.remove(block);

        // Add the fork block to the store (logs failures).
        if (!chain_.push(block, height++))
            return error::operation_failed;
    }

    height = base_height;

    for (const auto block: original)
    {
        // Original blocks remain valid at their original heights.
        set_height(block, height++);
        set_result(block, error::success);

        // Add the original block to the orphan pool.
        pool_.add(block);
    }

    notify_reorganize(fork_height, fork, original);
    return error::success;
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

// Utilities.
//-----------------------------------------------------------------------------
// static

// Summarize the difficulty of a chain of blocks.
hash_number orphan_pool_manager::fork_difficulty(list& fork)
{
    hash_number total;

    for (auto& block: fork)
        total += block->difficulty();

    return total;
}

// Calculate the blockchain height of the next block.
size_t orphan_pool_manager::to_height(size_t fork_height, size_t start_index)
{
    BITCOIN_ASSERT(fork_height <= max_size_t - start_index &&
        fork_height + start_index <= max_size_t - 1);

    // The height of the blockchain fork point plus zero-based orphan index.
    return fork_height + start_index + 1;
}

// Determine if the block has been validated for the height.
bool orphan_pool_manager::validated(block_const_ptr block, size_t height)
{
    return (block->metadata.validation_result == error::success &&
        block->metadata.validation_height == height);
}

void orphan_pool_manager::set_height(block_const_ptr block, size_t height)
{
    block->metadata.validation_height = height;
}

void orphan_pool_manager::set_result(block_const_ptr block, const code& ec)
{
    block->metadata.validation_result = ec;
}

// Remove the block from the fork.
void orphan_pool_manager::remove(list& fork, block_const_ptr block)
{
    const auto it = std::find(fork.begin(), fork.end(), block);

    if (it != fork.end())
        fork.erase(it);
}

} // namespace blockchain
} // namespace libbitcoin
