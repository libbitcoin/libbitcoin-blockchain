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

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace chain {

block_detail::block_detail(const block_type& actual_block)
  : block_hash_(hash_block_header(actual_block.header)),
    processed_(false), info_({ block_status::orphan, 0 }),
    actual_block_(std::make_shared<block_type>(actual_block))
{
}
block_detail::block_detail(const block_header_type& actual_block_header)
  : block_detail(block_type{ actual_block_header, {} })
{
}

block_type& block_detail::actual()
{
    return *actual_block_;
}
const block_type& block_detail::actual() const
{
    return *actual_block_;
}
std::shared_ptr<block_type> block_detail::actual_ptr() const
{
    return actual_block_;
}

void block_detail::mark_processed()
{
    processed_ = true;
}
bool block_detail::is_processed()
{
    return processed_;
}

const hash_digest& block_detail::hash() const
{
    return block_hash_;
}

void block_detail::set_info(const block_info& replace_info)
{
    info_ = replace_info;
}
const block_info& block_detail::info() const
{
    return info_;
}

void block_detail::set_error(const std::error_code& code)
{
    code_ = code;
}
const std::error_code& block_detail::error() const
{
    return code_;
}

orphans_pool::orphans_pool(size_t pool_size)
  : pool_(pool_size)
{
}

bool orphans_pool::add(block_detail_ptr incoming_block)
{
    BITCOIN_ASSERT(incoming_block);
    const auto& incomming_header = incoming_block->actual().header;
    for (auto current_block : pool_)
    {
        // No duplicates allowed.
        const auto& actual = current_block->actual().header;
        if (current_block->actual().header == incomming_header)
            return false;
    }

    pool_.push_back(incoming_block);
    return true;
}

block_detail_list orphans_pool::trace(block_detail_ptr end_block)
{
    BITCOIN_ASSERT(end_block);
    block_detail_list traced_chain;
    traced_chain.push_back(end_block);
    for (auto found = true; found;)
    {
        const auto& actual = traced_chain.back()->actual();
        const auto& previous_block_hash = actual.header.previous_block_hash;
        found = false;
        for (const auto current_block: pool_)
            if (current_block->hash() == previous_block_hash)
            {
                found = true;
                traced_chain.push_back(current_block);
                break;
            }
    }

    BITCOIN_ASSERT(traced_chain.size() > 0);
    std::reverse(traced_chain.begin(), traced_chain.end());
    return traced_chain;
}

block_detail_list orphans_pool::unprocessed()
{
    block_detail_list unprocessed_blocks;
    for (const auto current_block: pool_)
        if (!current_block->is_processed())
            unprocessed_blocks.push_back(current_block);

    // Earlier blocks come into pool first. Lets match that
    // Helps avoid fragmentation, but isn't neccessary
    std::reverse(unprocessed_blocks.begin(), unprocessed_blocks.end());
    return unprocessed_blocks;
}

void orphans_pool::remove(block_detail_ptr remove_block)
{
    BITCOIN_ASSERT(remove_block);
    auto it = std::find(pool_.begin(), pool_.end(), remove_block);
    BITCOIN_ASSERT(it != pool_.end());
    pool_.erase(it);
}

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
        auto process_block = process_queue_.back();
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
    block_detail_list replaced_slice;
    DEBUG_ONLY(bool success =) chain_.release(begin_index, replaced_slice);
    BITCOIN_ASSERT(success);

    // We add the arriving blocks first to the main chain because if
    // we add the blocks being replaced back to the pool first then
    // the we can push the arrival blocks off the bottom of the
    // circular buffer.
    // Then when we try to remove the block from the orphans pool,
    // if will fail to find it. I would rather not add an exception
    // there so that problems will show earlier.
    // All arrival_blocks should be blocks from the pool.
    auto arrival_index = fork_index;
    for (auto arrival_block: orphan_chain)
    {
        orphans_.remove(arrival_block);
        ++arrival_index;
        arrival_block->set_info({block_status::confirmed, arrival_index});
        chain_.append(arrival_block);
    }

    // Now add the old blocks back to the pool
    for (auto replaced_block: replaced_slice)
    {
        replaced_block->mark_processed();
        replaced_block->set_info({block_status::orphan, 0});
        orphans_.add(replaced_block);
    }

    notify_reorganize(fork_index, orphan_chain, replaced_slice);
}

void lazy_remove(block_detail_list& process_queue,
    block_detail_ptr remove_block)
{
    BITCOIN_ASSERT(remove_block);
    auto it = std::find(process_queue.begin(), process_queue.end(),
        remove_block);

    if (it != process_queue.end())
        process_queue.erase(it);

    remove_block->mark_processed();
}

void organizer::clip_orphans(block_detail_list& orphan_chain,
    size_t orphan_index, const std::error_code& invalid_reason)
{
    auto orphan_start = orphan_chain.begin() + orphan_index;
    // Remove from orphans pool
    for (auto it = orphan_start; it != orphan_chain.end(); ++it)
    {
        if (it == orphan_start)
            (*it)->set_error(invalid_reason);
        else
            (*it)->set_error(error::previous_block_invalid);

        (*it)->set_info({block_status::rejected, 0});
        orphans_.remove(*it);

        // Also erase from process_queue so we avoid trying to re-process
        // invalid blocks and remove try to remove non-existant blocks.
        lazy_remove(process_queue_, *it);
    }
    orphan_chain.erase(orphan_start, orphan_chain.end());
}

void organizer::notify_reorganize(size_t fork_point,
    const block_detail_list& orphan_chain,
    const block_detail_list& replaced_slice)
{
    // Strip out the meta-info, to convert to format which can
    // be passed to subscribe handlers
    // orphan_chain = arrival blocks
    // replaced slice = replaced chain
    blockchain::block_list arrival_blocks, replaced_blocks;
    for (auto arrival_block: orphan_chain)
        arrival_blocks.push_back(arrival_block->actual_ptr());

    for (auto replaced_block: replaced_slice)
        replaced_blocks.push_back(replaced_block->actual_ptr());

    reorganize_occured(fork_point, arrival_blocks, replaced_blocks);
}

} // namespace chain
} // namespace libbitcoin
