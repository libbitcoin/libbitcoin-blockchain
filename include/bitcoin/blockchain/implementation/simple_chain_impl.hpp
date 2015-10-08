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

#include <cstdint>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API simple_chain_impl
  : public simple_chain
{
public:
    simple_chain_impl(database& database);

    void append(block_detail::ptr incoming_block);
    hash_number sum_difficulty(uint64_t begin_index);
    bool release(uint64_t begin_index, block_detail::list& released_blocks);
    bool find_height(uint64_t& out_height,
        const hash_digest& search_block_hash);

private:
    database& database_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
