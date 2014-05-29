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

#include <bitcoin/types.hpp>
#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace blockchain {

constexpr size_t shard_max_entries = 1000000;

struct BCB_API hdb_shard_settings
{
    size_t scan_bitsize() const;
    size_t scan_size() const;
    size_t number_buckets() const;

    size_t total_key_size = 20;
    size_t sharded_bitsize = 8;
    size_t bucket_bitsize = 8;
    size_t row_value_size = 49;
};

class hdb_shard
{
public:
    BCB_API hdb_shard(mmfile& file, const hdb_shard_settings& settings);

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
      * Free up entries deleting them from the record.
      */
    BCB_API void unlink(size_t height);

    // read(prefix)
    // add(prefix, row)
    // sync(height)
    // unlink(height)

private:
    struct entry_row
    {
        address_bitset scan_key;
        data_chunk value;
    };
    typedef std::vector<entry_row> entry_row_list;

    // sync() related.
    void sort_rows();
    void reserve(size_t space_needed);
    void link(const size_t height, const position_type entry);

    // unlink() related.
    position_type entry_position(size_t height) const;
    size_t entry_size(const position_type entry) const;

    mmfile& file_;
    const hdb_shard_settings settings_;
    position_type entries_end_;
    entry_row_list rows_;
};

    } // namespace blockchain
} // namespace libbitcoin

#endif

