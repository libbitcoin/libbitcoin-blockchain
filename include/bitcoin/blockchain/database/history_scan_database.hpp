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
#ifndef LIBBITCOIN_BLOCKCHAIN_HDB_SETTINGS_HPP
#define LIBBITCOIN_BLOCKCHAIN_HDB_SETTINGS_HPP

#include <bitcoin/primitives.hpp>
#include <bitcoin/blockchain/database/hsdb_settings.hpp>
#include <bitcoin/blockchain/database/hsdb_shard.hpp>

namespace libbitcoin {
    namespace chain {

BCB_API void create_hsdb(const std::string& prefix,
    const hsdb_settings& settings=hsdb_settings());

enum class point_ident_type
{
    input,
    output
};

struct point_type
{
    point_ident_type ident;
    output_point point;
};

uint64_t compute_input_checksum(const output_point& previous_output);

class history_scan_database
{
public:
    typedef std::function<void (
        const point_type&, uint32_t, uint64_t)> read_function;

    BCB_API history_scan_database(const std::string& prefix);

    /**
      * Write method. Adds to in memory list of rows. sync() writes them.
      */
    BCB_API void add(const address_bitset& key,
        const point_type& point, uint32_t block_height, uint64_t value);

    /**
      * Write method. Flushes to disk and writes actual data.
      */
    BCB_API void sync(size_t height);

    /**
      * Delete method. Free up entries deleting them from the record.
      */
    BCB_API void unlink(size_t height);

    /**
      * Read method. Scan for rows with matching prefix.
      *
      * @param[in]  key             Bits for scanning through addresses.
      * @param[in]  read_func       Function for calling with results.
      * @code
      *  void read_func(
      *      const point_type& point, // Whether this point is input or output.
      *      uint32_t block_height,   // Height of block for this row.
      *      uint64_t value,          // Value (output) or checksum (input).
      *  );
      * @endcode
      * @param[in]  from_height     Starting height.
      */
    BCB_API void scan(const address_bitset& key,
        read_function read_func, size_t from_height);

private:
    typedef std::vector<mmfile> mmfile_list;
    typedef std::vector<hsdb_shard> shard_list;

    hsdb_shard& lookup(address_bitset key);
    address_bitset drop_prefix(address_bitset key);

    hsdb_settings settings_;
    mmfile_list shard_files_;
    shard_list shards_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

