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
#ifndef LIBBITCOIN_BLOCKCHAIN_STEALTH_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_STEALTH_DATABASE_HPP

#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/blockchain.hpp>
#include <bitcoin/blockchain/database/record_allocator.hpp>

namespace libbitcoin {
    namespace chain {

class stealth_database
{
public:
    typedef std::function<void (uint8_t*)> write_function;

    BCB_API stealth_database(
        const std::string& index_filename, const std::string& rows_filename);

    /**
     * Initialize a new stealth database.
     */
    BCB_API void initialize_new();

    /**
     * You must call start() before using the database.
     */
    BCB_API void start();

    /**
     * Linearly scans all entries starting at from_height.
     */
    BCB_API stealth_list scan(
        const stealth_prefix& prefix, const size_t from_height) const;

    /**
     * Add a stealth row to the database.
     */
    BCB_API void store(
        const script_type& stealth_script, const stealth_row& row);

    /**
     * Delete all rows after and including from_height.
     */
    BCB_API void unlink(const size_t from_height);

    /**
     * Synchronise storage with disk so things are consistent.
     * Should be done at the end of every block write.
     */
    BCB_API void sync();

private:
    void write_index();
    index_type read_index(const size_t from_height) const;

    index_type block_start_;

    /// Table used for jumping to rows by height.
    /// Resolves to a index within the rows.
    mmfile index_file_;
    record_allocator index_;

    /// Actual row entries containing stealth tx data.
    mmfile rows_file_;
    record_allocator rows_;
};

    } // namespace chain
} // namespace libbitcoin

#endif
