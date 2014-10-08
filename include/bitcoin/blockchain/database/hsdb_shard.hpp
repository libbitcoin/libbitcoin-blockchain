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
#ifndef LIBBITCOIN_BLOCKCHAIN_HSDB_SHARD_HPP
#define LIBBITCOIN_BLOCKCHAIN_HSDB_SHARD_HPP

#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>
#include <bitcoin/blockchain/database/hsdb_settings.hpp>

namespace libbitcoin {
    namespace chain {

class hsdb_shard
{
public:
    typedef std::function<void (const uint8_t*)> read_function;

    BCB_API hsdb_shard(mmfile& file, const hsdb_settings& settings);

    /**
      * Create database.
      */
    BCB_API void initialize_new();

    /**
      * Prepare database for usage.
      */
    BCB_API void start();

    /**
      * Write method. Adds to in memory list of rows. sync() writes them.
      */
    BCB_API void add(const address_bitset& scan_key, const data_chunk& value);

    /**
      * Write method. Flushes to disk and writes actual data.
      */
    BCB_API void sync(size_t height);

    /**
      * Delete method. Free up entries deleting them from the record.
      */
    BCB_API void unlink(size_t height);

    /**
      * Read method. Scan shard for rows with matching prefix.
      */
    BCB_API void scan(const address_bitset& key,
        read_function read, size_t from_height) const;

private:
    struct entry_row
    {
        address_bitset scan_key;
        data_chunk value;
    };
    typedef std::vector<entry_row> entry_row_list;

    position_type entry_position(size_t height) const;
    size_t calc_entry_size(const position_type entry) const;

    // sync() related.
    void sort_rows();
    void reserve(size_t space_needed);
    void link(const size_t height, const position_type entry);

    // scan() related.

    mmfile& file_;
    const hsdb_settings& settings_;
    position_type entries_end_;
    entry_row_list rows_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

