/*
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
#include "simple_chain_impl.hpp"

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
    namespace chain {

simple_chain_impl::simple_chain_impl(db_interface& interface)
  : interface_(interface)
{
}

void simple_chain_impl::append(block_detail_ptr incoming_block)
{
    const size_t last_height = interface_.blocks.last_height();
    BITCOIN_ASSERT(last_height != block_database::null_height);
    const block_type& actual_block = incoming_block->actual();
    interface_.push(actual_block);
}

size_t simple_chain_impl::find_height(const hash_digest& search_block_hash)
{
    auto result = interface_.blocks.get(search_block_hash);
    if (!result)
        return null_height;
    return result.height();
}

hash_number simple_chain_impl::sum_difficulty(size_t begin_index)
{
    hash_number total_work = 0;
    const size_t last_height = interface_.blocks.last_height();
    BITCOIN_ASSERT(last_height != block_database::null_height);
    for (size_t i = begin_index; i <= last_height; ++i)
    {
        uint32_t bits = interface_.blocks.get(i).header().bits;
        total_work += block_work(bits);
    }
    return total_work;
}

bool simple_chain_impl::release(size_t begin_index,
    block_detail_list& released_blocks)
{
    const size_t last_height = interface_.blocks.last_height();
    BITCOIN_ASSERT(last_height != block_database::null_height);
    BITCOIN_ASSERT(last_height > 0);
    for (size_t height = last_height; height >= begin_index; --height)
    {
        block_detail_ptr block =
            std::make_shared<block_detail>(interface_.pop());
        released_blocks.push_back(block);
    }
    return true;
}

    } // namespace chain
} // namespace libbitcoin
