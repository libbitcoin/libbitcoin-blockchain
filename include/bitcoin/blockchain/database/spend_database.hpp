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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_DATABASE_HPP

#include <bitcoin/error.hpp>
#include <bitcoin/primitives.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

/**
 * spend_database enables you to lookup the spend of an output point,
 * returning the input point. It is a simple map.
 */
class spend_database
{
public:
    typedef std::function<void (
        const std::error_code&, const input_point&)> fetch_handler;

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
     * Fetch input spend of an output point.
     *
     * @param[in]   outpoint        Representation of an output.
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,      // Status of operation
     *      const input_point& inpoint      // Spend of output
     *  );
     * @endcode
     */
    BCB_API void fetch(const output_point& outpoint,
        fetch_handler handle_fetch) const;

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

