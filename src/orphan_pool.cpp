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
#include <bitcoin/blockchain/orphan_pool.hpp>

#include <cstddef>
#include <bitcoin/blockchain/block_detail.hpp>

namespace libbitcoin {
namespace blockchain {

orphan_pool::orphan_pool(size_t size)
  : buffer_(size)
{
}

// TODO: there is no guard for entry here apart from nonexistence of the block.
bool orphan_pool::add(block_detail::ptr incoming_block)
{
    // No duplicates allowed.
    const auto& incoming_header = incoming_block->actual().header;
    for (const auto current_block: buffer_)
        if (current_block->actual().header == incoming_header)
            return false;

    buffer_.push_back(incoming_block);

    log::debug(LOG_BLOCKCHAIN)
        << "Orphan pool add (" << buffer_.size() << ") block ["
        << encode_hash(incoming_block->hash()) << "] previous ["
        << encode_hash(incoming_block->actual().header.previous_block_hash)
        << "]";

    return true;
}

void orphan_pool::remove(block_detail::ptr remove_block)
{
    const auto it = std::find(buffer_.begin(), buffer_.end(), remove_block);
    BITCOIN_ASSERT(it != buffer_.end());
    buffer_.erase(it);

    log::debug(LOG_BLOCKCHAIN)
        << "Orphan pool remove (" << buffer_.size() << ") block ["
        << encode_hash(remove_block->hash()) << "]";
}

block_detail::list orphan_pool::trace(block_detail::ptr end_block)
{
    block_detail::list traced_chain;
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

block_detail::list orphan_pool::unprocessed()
{
    block_detail::list unprocessed_blocks;
    for (const auto current_block: buffer_)
        if (!current_block->is_processed())
            unprocessed_blocks.push_back(current_block);

    // Earlier blocks come into pool first.
    // Helps avoid fragmentation, but isn't neccessary.
    std::reverse(unprocessed_blocks.begin(), unprocessed_blocks.end());
    return unprocessed_blocks;
}

} // namespace blockchain
} // namespace libbitcoin
