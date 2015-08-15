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
#ifndef LIBBITCOIN_BLOCKCHAIN_HISTORY_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_HISTORY_DATABASE_HPP

#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/blockchain.hpp>
#include <bitcoin/blockchain/database/multimap_records.hpp>

namespace libbitcoin {
namespace chain {

struct BCB_API history_statinfo
{
    /// Number of buckets used in the hashtable.
    /// load factor = addrs / buckets
    const size_t buckets;
    /// Total number of unique addresses in the database.
    const size_t addrs;
    /// Total number of rows across all addresses.
    const size_t rows;
};

/**
 * history_database is a multimap where the key is the Bitcoin address hash,
 * which returns several rows giving the history for that address.
 */
class BCB_API history_database
{
public:
    history_database(const boost::filesystem::path& lookup_filename,
        const boost::filesystem::path& rows_filename);

    /**
     * Initialize a new history database.
     */
    void create();

    /**
     * You must call start() before using the database.
     */
    void start();

    /**
     * Add another row value to the key. If key doesn't exist then
     * it will be created.
     */
    void add_output(const short_hash& key, const output_point& outpoint,
        const uint32_t output_height, const uint64_t value);

    /**
     * Add another row value to the key. If key doesn't exist then
     * it will be created.
     */
    void add_spend(const short_hash& key, const output_point& previous,
        const input_point& spend, const size_t spend_height);

    /**
     * Delete the last row that was added to key.
     */
    void delete_last_row(const short_hash& key);

    /**
     * Gets the output points, output values, corresponding input point
     * spends and the block heights associated with a Bitcoin address.
     * The returned history is a list of rows and a stop index.
     */
    history_list get(const short_hash& key, const size_t limit=0,
        const size_t from_height=0) const;

    /**
     * Synchonise with disk.
     */
    void sync();

    /**
     * Return statistical info about the database.
     */
    history_statinfo statinfo() const;

private:
    typedef htdb_record<short_hash> map_type;
    typedef multimap_records<short_hash> multimap_type;

    /// The hashtable used for looking up start index for a
    /// linked list by address hash.
    mmfile lookup_file_;
    htdb_record_header header_;
    record_allocator allocator_;
    map_type start_lookup_;

    mmfile rows_file_;
    record_allocator rows_;
    linked_records linked_rows_;

    multimap_type map_;
};

} // namespace chain
} // namespace libbitcoin

#endif

