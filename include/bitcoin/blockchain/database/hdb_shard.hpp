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
#ifndef LIBBITCOIN_BLOCKCHAIN_HDB_SHARD_HPP
#define LIBBITCOIN_BLOCKCHAIN_HDB_SHARD_HPP

#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace blockchain {

constexpr size_t shard_max_entries = 1000000;

struct hdb_shard_settings
{
};

class hdb_shard
{
public:
    BCB_API hdb_shard(mmfile& file, const hdb_shard_settings& settings);
    BCB_API void initialize_new();
    // initialize
    // read(prefix)
    // add(prefix, row)
    // sync(height)
    // unlink(height)
private:
    void reserve(size_t size);

    mmfile& file_;
    const hdb_shard_settings settings_;
};

    } // namespace blockchain
} // namespace libbitcoin

#endif

