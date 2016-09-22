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

#define NAME "orphan_pool_manager"

orphan_pool_manager::orphan_pool_manager(threadpool& pool, simple_chain& chain,
    const settings& settings)
  : chain_(chain),
    validator_(pool, settings.use_testnet_rules, checkpoints_, chain_),
    testnet_rules_(settings.use_testnet_rules),
    checkpoints_(checkpoint::sort(settings.checkpoints)),
    stopped_(true),
    orphan_pool_(settings.block_pool_capacity),
    subscriber_(std::make_shared<reorganize_subscriber>(pool, NAME))
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

// Verify (invoked from chain_work) sequence.
//-----------------------------------------------------------------------------

// This verifies the block at new_chain[orphan_index]
code orphan_pool_manager::verify(size_t fork_height,
    const block_const_ptr_list& new_chain, size_t orphan_index) const
{
    if (stopped())
        return error::service_stopped;

    // TODO: reenable once implemented.
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    return error::validate_inputs_failed;
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    BITCOIN_ASSERT(orphan_index < new_chain.size());
    const auto height = compute_height(fork_height, orphan_index);
    const auto block = new_chain[orphan_index];

    // Checks that are independent of the chain.
    validator_.check(block, nullptr);

    // Checks that are dependent on chain state.
    validator_.accept(block, nullptr);

    // Checks that include script validation.
    validator_.connect(block, nullptr);

    return error::success;
}

// Get the blockchain height of the next block (bottom of orphan chain).
size_t orphan_pool_manager::compute_height(size_t fork_height,
    size_t orphan_index)
{
    // The height of the blockchain fork point plus zero-based orphan index.
    BITCOIN_ASSERT(fork_height <= max_size_t - orphan_index);
    auto height = fork_height + orphan_index;

    // Needs to be incremented to account for the zero-based index.
    BITCOIN_ASSERT(height <= max_size_t - 1);
    return ++height;
}

// Organize.
//-----------------------------------------------------------------------------

// This is called on every full_chain_impl::do_store() call.
code orphan_pool_manager::reorganize(block_const_ptr block)
{
    if (!orphan_pool_.add(block))
        return error::duplicate;

    process_queue_ = orphan_pool_.unprocessed();

    while (!process_queue_.empty() && !stopped())
    {
        const auto process_block = process_queue_.back();
        process_queue_.pop_back();
        process(process_block);
    }

    return error::success;
}

void orphan_pool_manager::process(block_const_ptr block)
{
    // Trace the chain in the orphan pool
    auto new_chain = orphan_pool_.trace(block);
    BITCOIN_ASSERT(!new_chain.empty());

    uint64_t fork_height64;
    const auto& previous_hash = new_chain.front()->header.previous_block_hash;

    // Verify the blocks in the orphan chain.
    if (chain_.get_height(fork_height64, previous_hash))
    {
        BITCOIN_ASSERT(fork_height64 <= max_size_t);
        const auto fork_height = static_cast<size_t>(fork_height64);
        replace_chain(new_chain, fork_height);
    }

    // Don't mark all new_chain as processed here because there might be
    // a winning fork from an earlier block.
    block->metadata.processed_orphan = true;
}

void orphan_pool_manager::chain_work(hash_number& work,
    block_const_ptr_list& new_chain, size_t fork_height)
{
    work = 0;

    // Verify the new chain before allowing the reorg.
    for (size_t index = 0; index < new_chain.size(); ++index)
    {
        // This verifies the block at new_chain[index]
        const auto error_code = verify(fork_height, new_chain, index);

        if (error_code)
        {
            // If invalid block info is also set for the block.
            if (error_code != error::service_stopped)
            {
                log::warning(LOG_BLOCKCHAIN)
                    << "Invalid block ["
                    << encode_hash(new_chain[index]->hash())
                    << "] " << error_code.message();
            }

            // Index block is invalid, remove it and all after.
            clip_orphans(new_chain, index, error_code);

            // Stop summing work once we discover an invalid block.
            break;
        }

        const auto bits = new_chain[index]->header.bits;
        work += block::difficulty(bits);
    }
}

void orphan_pool_manager::replace_chain(block_const_ptr_list& new_chain,
    size_t fork_height)
{
    // Any invalid blocks are removed from new_chain, remaining work returned.
    hash_number new_work;
    chain_work(new_work, new_chain, fork_height);

    // For work comparison each branch starts one block above the fork height.
    auto from_height = fork_height + 1;

    hash_number old_work;
    const auto result = chain_.get_difficulty(old_work, from_height);

    if (!result)
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure getting difficulty from [" << from_height << "]";
        return;
    }

    if (new_work <= old_work)
    {
        log::debug(LOG_BLOCKCHAIN)
            << "Insufficient work to reorganize from [" << from_height << "]";
        return;
    }

    block_const_ptr_list old_chain;

    if (!chain_.pop_from(old_chain, from_height))
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure reorganizing from [" << from_height << "]";
        return;
    }

    if (!old_chain.empty())
    {
        log::info(LOG_BLOCKCHAIN)
            << "Reorganizing from block " << from_height << " to "
            << from_height + old_chain.size() << "]";
    }

    // Push the new_chain to the blockchain first because if the old is pushed
    // back to the orphan pool first then it could push the new blocks off the
    // end of the circular buffer.

    // Replace! Switch!
    // Remove new_chain blocks from the orphan pool and add them to the store.
    for (const auto block: new_chain)
    {
        orphan_pool_.remove(block);

        // THIS IS THE DATABASE BLOCK WRITE AND INDEX OPERATION.
        if (!chain_.push(block, from_height))
        {
            log::error(LOG_BLOCKCHAIN)
                << "Failure storing block [" << from_height << "]";
            return;
        }

        // Provides height parameter for blockchain.store() handler to return.
        block->metadata.validation_height = from_height++;
    }

    // Add old_chain to the orphan pool (as processed with orphan height).
    for (const auto block: old_chain)
    {
        block->metadata.processed_orphan = true;
        orphan_pool_.add(block);
    }

    notify_reorganize(fork_height, new_chain, old_chain);
}

void orphan_pool_manager::clip_orphans(block_const_ptr_list& new_chain,
    size_t orphan_index, const code& reason) 
{
    BITCOIN_ASSERT(orphan_index < new_chain.size());
    const auto start = new_chain.begin() + orphan_index;

    // Remove from orphans pool and process queue.
    for (auto it = start; it != new_chain.end(); ++it)
    {
        const auto ec = it == start ? reason : error::previous_block_invalid;
        (*it)->metadata.validation_result = ec;
        (*it)->metadata.processed_orphan = true;
        remove_processed(*it);
        orphan_pool_.remove(*it);
    }

    new_chain.erase(start, new_chain.end());
}

void orphan_pool_manager::remove_processed(block_const_ptr block)
{
    auto it = std::find(process_queue_.begin(), process_queue_.end(), block);

    if (it != process_queue_.end())
        process_queue_.erase(it);
}

void orphan_pool_manager::notify_reorganize(size_t fork_height,
    const block_const_ptr_list& new_chain,
    const block_const_ptr_list& old_chain)
{
    subscriber_->relay(error::success, fork_height, new_chain, old_chain);
}

// Utilities.
//-----------------------------------------------------------------------------

void orphan_pool_manager::filter_orphans(get_data_ptr message) const
{
    orphan_pool_.filter(message);
}

void orphan_pool_manager::subscribe_reorganize(reorganize_handler handler)
{
    subscriber_->subscribe(handler, error::service_stopped, 0, {}, {});
}

} // namespace blockchain
} // namespace libbitcoin
