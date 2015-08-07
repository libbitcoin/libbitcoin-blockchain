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
#include <bitcoin/blockchain/orphans_pool.hpp>

#include <cstddef>
#include <bitcoin/blockchain/block_detail.hpp>

namespace libbitcoin {
namespace chain {

orphans_pool::orphans_pool(size_t size)
  : buffer_(size)
{
}

orphans_pool::~orphans_pool()
{
    // This was reportedly required for use with circular_buffer.
}

bool orphans_pool::empty() const
{
    return buffer_.empty();
}

size_t orphans_pool::size() const
{
    return buffer_.size();
}

bool orphans_pool::add(block_detail_ptr incoming_block)
{
    BITCOIN_ASSERT(incoming_block);
    const auto& incomming_header = incoming_block->actual().header;
    for (auto current_block: buffer_)
    {
        // No duplicates allowed.
        const auto& actual = current_block->actual().header;
        if (current_block->actual().header == incomming_header)
            return false;
    }

    buffer_.push_back(incoming_block);

    log_debug(LOG_BLOCKCHAIN)
        << "Orphan pool add (" << buffer_.size() << ")";

    return true;
}

void orphans_pool::remove(block_detail_ptr remove_block)
{
    BITCOIN_ASSERT(remove_block);
    const auto it = std::find(buffer_.begin(), buffer_.end(), remove_block);
    BITCOIN_ASSERT(it != buffer_.end());
    buffer_.erase(it);

    log_debug(LOG_BLOCKCHAIN)
        << "Orphan pool remove (" << buffer_.size() << ")";
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
        for (const auto current_block: buffer_)
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
    for (const auto current_block: buffer_)
        if (!current_block->is_processed())
            unprocessed_blocks.push_back(current_block);

    // Earlier blocks come into pool first.
    // Helps avoid fragmentation, but isn't neccessary.
    std::reverse(unprocessed_blocks.begin(), unprocessed_blocks.end());
    return unprocessed_blocks;
}

} // namespace chain
} // namespace libbitcoin
