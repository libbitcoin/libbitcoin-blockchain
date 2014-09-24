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

#include <bitcoin/bitcoin/error.hpp>
#include <bitcoin/bitcoin/primitives.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

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

/**
 * spend_database enables you to lookup the spend of an output point,
 * returning the input point. It is a simple map.
 */
class spend_database
{
public:
    BCB_API spend_database(const std::string& filename);

    /**
     * Initialize a new spend database.
     */
    BCB_API void initialize_new();

    /**
     * You must call start() before using the database.
     */
    BCB_API void start();

    /**
     * Get input spend of an output point.
     */
    BCB_API spend_result get(const output_point& outpoint) const;

    /**
     * Store a spend in the database.
     */
    BCB_API void store(
        const output_point& outpoint, const input_point& spend);

    /**
     * Delete outpoint spend item from database.
     */
    BCB_API void remove(const output_point& outpoint);

    /**
     * Synchronise storage with disk so things are consistent.
     * Should be done at the end of every block write.
     */
    BCB_API void sync();

private:
    typedef htdb_record<hash_digest> map_type;

    /// The hashtable used for looking up inpoint spends by outpoint.
    mmfile file_;
    htdb_record_header header_;
    record_allocator allocator_;
    map_type map_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

