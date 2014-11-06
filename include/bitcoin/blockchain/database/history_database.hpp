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
#ifndef LIBBITCOIN_BLOCKCHAIN_HISTORY_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_HISTORY_DATABASE_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/multimap_records.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

/**
 * history_database is a multimap where the key is the Bitcoin address hash,
 * which returns several rows giving the history for that address.
 */
class history_database
{
public:
    BCB_API history_database(
        const std::string& lookup_filename, const std::string& rows_filename);

    /**
     * Initialize a new history database.
     */
    BCB_API void initialize_new();

    /**
     * You must call start() before using the database.
     */
    BCB_API void start();

    /**
     * Add another row value to the key. If key doesn't exist then
     * it will be created.
     */
    BCB_API void add_row(
        const short_hash& key, const output_point& outpoint,
        const uint32_t output_height, const uint64_t value);

    /**
     * Add a spend to an existing row.
     * Returns false if the previous_output does not exist.
     */
    BCB_API void add_spend(
        const short_hash& key, const output_point& previous,
        const input_point& spend, const size_t spend_height);

    /**
     * Delete the last row that was added to key.
     */
    BCB_API void delete_last_row(const short_hash& key);

    /**
     * Gets the output points, output values, corresponding input point
     * spends and the block heights associated with a Bitcoin address.
     * The returned history is a list of rows and a stop index.
     */
    BCB_API history_list get(const short_hash& key,
        const size_t limit=0, const size_t from_height=0) const;

    /**
     * Synchonise with disk.
     */
    BCB_API void sync();

private:
    typedef htdb_record<short_hash> map_type;
    typedef multimap_records<short_hash> multimap_type;

    /// The hashtable used for looking start index for a
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

