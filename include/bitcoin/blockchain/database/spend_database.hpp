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
#ifndef LIBBITCOIN_BLOCKCHAIN_SPEND_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_SPEND_DATABASE_HPP

#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>

namespace libbitcoin {
namespace blockchain {

class spend_result
{
public:
    spend_result(const record_type record);

    /**
     * Test whether the result exists, return false otherwise.
     */
    BCB_API operator bool() const;

    /**
     * Transaction hash for spend.
     */
    BCB_API hash_digest hash() const;

    /**
     * Index of input within transaction for spend.
     */
    BCB_API uint32_t index() const;

private:
    const record_type record_;
};

struct spend_statinfo
{
    /// Number of buckets used in the hashtable.
    /// load factor = rows / buckets
    const size_t buckets;
    /// Total number of spend rows.
    const size_t rows;
};

/**
 * spend_database enables you to lookup the spend of an output point,
 * returning the input point. It is a simple map.
 */
class spend_database
{
public:
    BCB_API spend_database(const boost::filesystem::path& filename);

    /**
     * Initialize a new spend database.
     */
    BCB_API void create();

    /**
     * You must call start() before using the database.
     */
    BCB_API void start();

    /**
     * Get input spend of an output point.
     */
    BCB_API spend_result get(const chain::output_point& outpoint) const;

    /**
     * Store a spend in the database.
     */
    BCB_API void store(
        const chain::output_point& outpoint, const chain::input_point& spend);

    /**
     * Delete outpoint spend item from database.
     */
    BCB_API void remove(const chain::output_point& outpoint);

    /**
     * Synchronise storage with disk so things are consistent.
     * Should be done at the end of every block write.
     */
    BCB_API void sync();

    /**
     * Return statistical info about the database.
     */
    BCB_API spend_statinfo statinfo() const;

private:
    typedef htdb_record<hash_digest> map_type;

    /// The hashtable used for looking up inpoint spends by outpoint.
    mmfile file_;
    htdb_record_header header_;
    record_allocator allocator_;
    map_type map_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
