/**
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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

#include <cstddef>
#include <system_error>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/blockchain.hpp>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/orphans_pool.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>

namespace libbitcoin {
namespace blockchain {

organizer::organizer(orphans_pool& orphans, simple_chain& chain)
  : orphans_(orphans), chain_(chain)
{
}

void organizer::start()
{
    // Load unprocessed blocks
    process_queue_ = orphans_.unprocessed();

    // As we loop, we pop blocks off and process them
    while (!process_queue_.empty())
    {
        const auto process_block = process_queue_.back();
        process_queue_.pop_back();

        // process() can remove blocks from the queue too
        process(process_block);
    }
}

void organizer::process(block_detail_ptr process_block)
{
    BITCOIN_ASSERT(process_block);

    // Trace the chain in the orphan pool
    auto orphan_chain = orphans_.trace(process_block);
    BITCOIN_ASSERT(orphan_chain.size() >= 1);

    const auto& hash = orphan_chain[0]->actual().header.previous_block_hash;
    const auto fork_index = chain_.find_height(hash);

    if (fork_index != simple_chain::null_height)
        replace_chain(fork_index, orphan_chain);

    // Don't mark all orphan_chain as processed here because there might be
    // a winning fork from an earlier block
    process_block->mark_processed();
}

void organizer::replace_chain(size_t fork_index,
    block_detail_list& orphan_chain)
{
    hash_number orphan_work = 0;
    for (size_t orphan = 0; orphan < orphan_chain.size(); ++orphan)
    {
        const auto invalid_reason = verify(fork_index, orphan_chain, orphan);
        if (invalid_reason)
        {
            const auto& header = orphan_chain[orphan]->actual().header;
            log_warning(LOG_VALIDATE) << "Invalid block ["
                << encode_base16(header.hash()) << "] "
                << invalid_reason.value();

            // Block is invalid, clip the orphans.
            clip_orphans(orphan_chain, orphan, invalid_reason);

            // Stop summing work once we discover an invalid block
            break;
        }

        const auto& orphan_block = orphan_chain[orphan]->actual();
        orphan_work += block_work(orphan_block.header.bits);
    }

    // All remaining blocks in orphan_chain should all be valid now
    // Compare the difficulty of the 2 forks (original and orphan)
    const auto begin_index = fork_index + 1;
    const auto main_work = chain_.sum_difficulty(begin_index);
    if (orphan_work <= main_work)
        return;

    // Replace! Switch!
    block_detail_list released_blocks;
    DEBUG_ONLY(bool success =) chain_.release(begin_index, released_blocks);
    BITCOIN_ASSERT(success);

    if (!released_blocks.empty())
        log_warning(LOG_BLOCKCHAIN) << "Reorganizing blockchain ["
            << begin_index << ", " << released_blocks.size() << "]";

    // We add the arriving blocks first to the main chain because if
    // we add the blocks being replaced back to the pool first then
    // the we can push the arrival blocks off the bottom of the
    // circular buffer.
    // Then when we try to remove the block from the orphans pool,
    // if will fail to find it. I would rather not add an exception
    // there so that problems will show earlier.
    // All arrival_blocks should be blocks from the pool.
    auto arrival_index = fork_index;
    for (const auto arrival_block: orphan_chain)
    {
        orphans_.remove(arrival_block);
        ++arrival_index;
        arrival_block->set_info({block_status::confirmed, arrival_index});
        chain_.append(arrival_block);
    }

    // Now add the old blocks back to the pool
    for (const auto replaced_block: released_blocks)
    {
        replaced_block->mark_processed();
        replaced_block->set_info({block_status::orphan, 0});
        orphans_.add(replaced_block);
    }

    notify_reorganize(fork_index, orphan_chain, released_blocks);
}

static void lazy_remove(block_detail_list& process_queue,
    block_detail_ptr remove_block)
{
    BITCOIN_ASSERT(remove_block);
    const auto it = std::find(process_queue.begin(), process_queue.end(),
        remove_block);
    if (it != process_queue.end())
        process_queue.erase(it);

    remove_block->mark_processed();
}

void organizer::clip_orphans(block_detail_list& orphan_chain,
    size_t orphan_index, const std::error_code& invalid_reason)
{
    // Remove from orphans pool.
    auto orphan_start = orphan_chain.begin() + orphan_index;
    for (auto it = orphan_start; it != orphan_chain.end(); ++it)
    {
        if (it == orphan_start)
            (*it)->set_error(invalid_reason);
        else
            (*it)->set_error(error::previous_block_invalid);

        const static size_t height = 0;
        const block_info info{ block_status::rejected, height };
        (*it)->set_info(info);
        orphans_.remove(*it);

        // Also erase from process_queue so we avoid trying to re-process
        // invalid blocks and remove try to remove non-existant blocks.
        lazy_remove(process_queue_, *it);
    }

    orphan_chain.erase(orphan_start, orphan_chain.end());
}

void organizer::notify_reorganize(size_t fork_point,
    const block_detail_list& orphan_chain,
    const block_detail_list& replaced_chain)
{
    // Strip out meta-info, converting to format passed to subscribe handlers.
    blockchain::block_list arrival_blocks, replaced_blocks;
    for (const auto arrival_block: orphan_chain)
        arrival_blocks.push_back(arrival_block->actual_ptr());

    for (const auto replaced_block: replaced_chain)
        replaced_blocks.push_back(replaced_block->actual_ptr());

    reorganize_occured(fork_point, arrival_blocks, replaced_blocks);
}

} // namespace blockchain
} // namespace libbitcoin
