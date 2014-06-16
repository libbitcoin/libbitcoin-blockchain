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
#ifndef LIBBITCOIN_BLOCKCHAIN_LEVELDB_CHAIN_KEEPER_H
#define LIBBITCOIN_BLOCKCHAIN_LEVELDB_CHAIN_KEEPER_H

#include <bitcoin/blockchain/blockchain_impl.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include "blockchain_common.hpp"

namespace libbitcoin {
    namespace chain {

class simple_chain_impl
  : public simple_chain
{
public:
    simple_chain_impl(blockchain_common_ptr common, leveldb_databases db);
    void append(block_detail_ptr incoming_block);
    int find_index(const hash_digest& search_block_hash);
    big_number sum_difficulty(size_t begin_index);
    bool release(size_t begin_index,
        block_detail_list& released_blocks);

private:
    bool clear_transaction_data(leveldb_transaction_batch& batch,
        const transaction_type& remove_tx);

    blockchain_common_ptr common_;
    leveldb_databases db_;
};

    } // namespace chain
} // namespace libbitcoin

#endif
