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
#ifndef LIBBITCOIN_BLOCKCHAIN_SIMPLE_CHAIN_IMPL_HPP
#define LIBBITCOIN_BLOCKCHAIN_SIMPLE_CHAIN_IMPL_HPP

#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/db_interface.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>

namespace libbitcoin {
namespace chain {

class BCB_API simple_chain_impl
  : public simple_chain
{
public:
    simple_chain_impl(db_interface& database);
    void append(block_detail_ptr incoming_block);
    size_t find_height(const hash_digest& search_block_hash);
    hash_number sum_difficulty(size_t begin_index);
    bool release(size_t begin_index, block_detail_list& released_blocks);

private:
    db_interface& interface_;
};

} // namespace chain
} // namespace libbitcoin

#endif
