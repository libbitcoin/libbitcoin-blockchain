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
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>

namespace libbitcoin {
namespace blockchain {

simple_chain_impl::simple_chain_impl(database& database)
  : database_(database)
{
}

void simple_chain_impl::append(block_detail::ptr incoming_block)
{
    BITCOIN_ASSERT(incoming_block);
    database_.push(incoming_block->actual());
}

hash_number simple_chain_impl::sum_difficulty(uint64_t begin_index)
{
    hash_number total_work = 0;

    size_t last_height;
    if (!database_.blocks.top(last_height))
        return total_work;

    for (uint64_t height = begin_index; height <= last_height; ++height)
    {
        const auto bits = database_.blocks.get(height).header().bits;
        total_work += block_work(bits);
    }

    return total_work;
}

bool simple_chain_impl::release(uint64_t begin_index,
    block_detail::list& released_blocks)
{
    size_t last_height;
    if (!database_.blocks.top(last_height))
        return false;

    for (uint64_t height = last_height; height >= begin_index; --height)
    {
        const auto block = std::make_shared<block_detail>(database_.pop());
        released_blocks.push_back(block);
    }

    return true;
}

bool simple_chain_impl::find_height(uint64_t& out_height,
    const hash_digest& search_block_hash)
{
    auto result = database_.blocks.get(search_block_hash);
    if (!result)
        return false;

    out_height = result.height();
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
