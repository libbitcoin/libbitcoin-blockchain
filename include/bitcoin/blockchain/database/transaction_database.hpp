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

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/htdb_slab.hpp>
#include <bitcoin/blockchain/database/record_allocator.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

struct transaction_metainfo
{
    size_t height, index;
};

class transaction_result
{
public:
    transaction_result(const slab_type slab);

    /**
     * Test whether the result exists, return false otherwise.
     */
    BCB_API operator bool() const;

    /**
     * Height of the block which includes this transaction.
     */
    BCB_API size_t height() const;

    /**
     * Index of transaction within a block.
     */
    BCB_API size_t index() const;

    /**
     * Actual transaction itself.
     */
    BCB_API transaction_type transaction() const;

private:
    const slab_type slab_;
};

/**
 * transaction_database enables lookups of transactions by hash.
 * An alternative and faster method is lookup from a unique index
 * that is assigned upon storage.
 * This is so we can quickly reconstruct blocks given a list of tx indexes
 * belonging to that block. These are stored with the block.
 */
class transaction_database
{
public:
    BCB_API transaction_database(
        const std::string& map_filename, const std::string& index_filename);

    /**
     * Initialize a new transaction database.
     */
    BCB_API void initialize_new();

    /**
     * You must call start() before using the database.
     */
    BCB_API void start();

    /**
     * Fetch transaction from its unique index. Does an intermediate
     * lookup in another table to find its position on disk.
     */
    BCB_API transaction_result get(const index_type index) const;

    /**
     * Fetch transaction from its hash.
     */
    BCB_API transaction_result get(const hash_digest& hash) const;

    /**
     * Store a transaction in the database. Returns a unique index
     * which can be used to reference the transaction.
     */
    BCB_API index_type store(
        const transaction_metainfo& info, const transaction_type& tx);

    /**
     * Synchronise storage with disk so things are consistent.
     * Should be done at the end of every block write.
     */
    BCB_API void sync();

private:
    typedef htdb_slab<hash_digest> map_type;

    /// Write position of tx and return assigned index
    index_type write_position(const position_type position);
    /// Use intermediate records table to find tx position.
    position_type read_position(const index_type index) const;

    /// The hashtable used for looking up txs by hash.
    mmfile map_file_;
    htdb_slab_header header_;
    slab_allocator allocator_;
    map_type map_;

    /// Record table used for looking up tx position from a unique index.
    mmfile index_file_;
    record_allocator index_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

