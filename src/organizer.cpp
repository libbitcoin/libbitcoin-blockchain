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
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/block.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/orphan_pool.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>
#include <bitcoin/blockchain/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;

#define NAME "organizer"

organizer::organizer(threadpool& pool, simple_chain& chain,
    const settings& settings)
  : stopped_(true),
    testnet_rules_(settings.use_testnet_rules),
    checkpoints_(checkpoint::sort(settings.checkpoints)),
    chain_(chain),
    validator_(pool, testnet_rules_, checkpoints_, chain_),
    orphan_pool_(settings.block_pool_capacity),
    subscriber_(std::make_shared<reorganize_subscriber>(pool, NAME))
{
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

void organizer::start()
{
    stopped_ = false;
    subscriber_->start();
}

void organizer::stop()
{
    stopped_ = true;
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, 0, {}, {});
}

bool organizer::stopped() const
{
    return stopped_;
}

////validate_block_impl validator(fork_height, new_chain, orphan_index,
////    block, height, testnet_rules_, checkpoints_, chain_);

// Verify (invoked from chain_work) sequence.
//-----------------------------------------------------------------------------

// This verifies the block at new_chain[orphan_index]->actual()
code organizer::verify(size_t fork_height, const detail_list& new_chain,
    size_t orphan_index)
{
    if (stopped())
        return error::service_stopped;

    // TODO: reenable once implemented.
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    return error::validate_inputs_failed;
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    BITCOIN_ASSERT(orphan_index < new_chain.size());
    const auto height = compute_height(fork_height, orphan_index);
    const auto block = new_chain[orphan_index]->actual();

    // Checks that are independent of the chain.
    validator_.check(block, nullptr);

    // Checks that are dependent on chain state.
    validator_.accept(block, nullptr);

    // Checks that include script validation.
    validator_.connect(block, nullptr);

    return error::success;
}

// Get the blockchain height of the next block (bottom of orphan chain).
size_t organizer::compute_height(size_t fork_height, size_t orphan_index)
{
    // The height of the blockchain fork point plus zero-based orphan index.
    BITCOIN_ASSERT(fork_height <= max_size_t - orphan_index);
    auto height = fork_height + orphan_index;

    // Needs to be incremented to account for the zero-based index.
    BITCOIN_ASSERT(height <= max_size_t - 1);
    return ++height;
}

// Add and organize.
//-----------------------------------------------------------------------------

// This is called on every block_chain_impl::do_store() call.
bool organizer::add(block_detail::ptr block)
{
    return orphan_pool_.add(block);
}

// This is called on every block_chain_impl::do_store() call.
void organizer::organize()
{
    process_queue_ = orphan_pool_.unprocessed();

    while (!process_queue_.empty() && !stopped())
    {
        const auto process_block = process_queue_.back();
        process_queue_.pop_back();
        process(process_block);
    }
}

void organizer::filter_orphans(message::get_data::ptr message)
{
    orphan_pool_.filter(message);
}

void organizer::process(detail_ptr block)
{
    // Trace the chain in the orphan pool
    auto new_chain = orphan_pool_.trace(block);
    BITCOIN_ASSERT(new_chain.size() >= 1);

    uint64_t fork_height64;
    const auto& hash = new_chain.front()->actual()->header.previous_block_hash;

    // Verify the blocks in the orphan chain.
    if (chain_.get_height(fork_height64, hash))
    {
        BITCOIN_ASSERT(fork_height64 <= max_size_t);
        const auto fork_height = static_cast<size_t>(fork_height64);
        replace_chain(fork_height, new_chain);
    }

    // Don't mark all new_chain as processed here because there might be
    // a winning fork from an earlier block.
    block->set_processed();
}

hash_number organizer::chain_work(size_t fork_height, detail_list& new_chain)
{
    hash_number work;

    // Verify the new chain before allowing the reorg.
    for (size_t index = 0; index < new_chain.size(); ++index)
    {
        // This verifies the block at new_chain[index]->actual()
        const auto error_code = verify(fork_height, new_chain, index);

        if (error_code)
        {
            // If invalid block info is also set for the block.
            if (error_code != error::service_stopped)
            {
                log::warning(LOG_BLOCKCHAIN)
                    << "Invalid block ["
                    << encode_hash(new_chain[index]->actual()->hash())
                    << "] " << error_code.message();
            }

            // Index block is invalid, remoev it and all after.
            clip_orphans(new_chain, index, error_code);

            // Stop summing work once we discover an invalid block.
            break;
        }

        const auto bits = new_chain[index]->actual()->header.bits;
        work += bc::chain::block::work(bits);
    }

    // Return no error, just a trimmed chain.
    return work;
}

void organizer::replace_chain(size_t fork_height, detail_list& new_chain)
{
    // Any invalid blocks are removed from new_chain, remaining work returned.
    const auto new_work = chain_work(fork_height, new_chain);

    // For work comparison each branch starts one block above the fork height.
    auto height = fork_height + 1;

    hash_number old_work;
    auto result = chain_.get_difficulty(old_work, height);

    if (!result)
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure getting difficulty for [" << height << "]";
        return;
    }

    if (new_work <= old_work)
    {
        log::debug(LOG_BLOCKCHAIN)
            << "Insufficient work to reorganize at [" << height << "]";
        return;
    }

    detail_list old_chain;

    if (!chain_.pop_from(old_chain, height))
    {
        log::error(LOG_BLOCKCHAIN)
            << "Failure reorganizing from [" << height << "]";
        return;
    }

    if (!old_chain.empty())
    {
        log::info(LOG_BLOCKCHAIN)
            << "Reorganizing [" << height << ", " << old_chain.size() << "]";
    }

    // Push the new_chain to the blockchain first because if the old is pushed
    // back to the orphan pool first then it could push the new blocks off the
    // end of the circular buffer.

    // Replace! Switch!
    // Remove new_chain blocks from the orphan pool and add them to the store.
    for (const auto block: new_chain)
    {
        orphan_pool_.remove(block);

        // Indicates the block is not an orphan.
        block->set_height(height);

        // THIS IS THE DATABASE BLOCK WRITE AND INDEX OPERATION.
        if (!chain_.push(block))
        {
            log::error(LOG_BLOCKCHAIN)
                << "Failure storing block [" << height << "]";
            return;
        }

        // Move to next height.
        ++height;
    }

    // Add old_chain to the orphan pool (as processed with orphan height).
    for (const auto block: old_chain)
    {
        block->set_processed();
        orphan_pool_.add(block);
    }

    notify_reorganize(fork_height, new_chain, old_chain);
}

void organizer::clip_orphans(detail_list& new_chain, size_t orphan_index,
    const code& reason)
{
    BITCOIN_ASSERT(orphan_index < new_chain.size());
    const auto start = new_chain.begin() + orphan_index;

    // Remove from orphans pool and process queue.
    for (auto it = start; it != new_chain.end(); ++it)
    {
        (*it)->set_error(it == start ? reason : error::previous_block_invalid);
        (*it)->set_processed();
        remove_processed(*it);
        orphan_pool_.remove(*it);
    }

    new_chain.erase(start, new_chain.end());
}

void organizer::remove_processed(detail_ptr block)
{
    auto it = std::find(process_queue_.begin(), process_queue_.end(), block);

    if (it != process_queue_.end())
        process_queue_.erase(it);
}

void organizer::subscribe_reorganize(reorganize_handler handler)
{
    subscriber_->subscribe(handler, error::service_stopped, 0, {}, {});
}

void organizer::notify_reorganize(size_t fork_height,
    const detail_list& new_chain, const detail_list& old_chain)
{
    typedef message::block_message::ptr_list list;

    const auto map = [](const detail_ptr& detail)
    {
        return detail->actual();
    };

    list arrivals(new_chain.size());
    list removals(old_chain.size());
    std::transform(new_chain.begin(), new_chain.end(), arrivals.begin(), map);
    std::transform(old_chain.begin(), old_chain.end(), removals.begin(), map);
    subscriber_->relay(error::success, fork_height, arrivals, removals);
}

} // namespace blockchain
} // namespace libbitcoin
