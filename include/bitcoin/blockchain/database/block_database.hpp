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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_DATABASE_HPP

#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/htdb_slab.hpp>
#include <bitcoin/blockchain/database/record_allocator.hpp>

namespace libbitcoin {
namespace chain {

class block_result
{
public:
    block_result(const slab_type slab);

    /**
     * Test whether the result exists, return false otherwise.
     */
    BCB_API operator bool() const;

    /**
     * Read block header.
     */
    BCB_API block_header_type header() const;

    /**
     * The height of this block in the blockchain.
     */
    BCB_API size_t height() const;

    /**
     * Read the number of transactions in this block.
     */
    BCB_API size_t transactions_size() const;

    /**
     * Read a transaction hash where i < transactions_size().
     */
    BCB_API hash_digest transaction_hash(size_t i) const;

private:
    const slab_type slab_;
};

typedef std::vector<index_type> transaction_index_list;

/**
 * Stores block_headers each with a list of transaction indexes.
 * Lookup possible by hash or height.
 */
class block_database
{
public:
    static BC_CONSTEXPR size_t null_height = bc::max_size_t;

    BCB_API block_database(const boost::filesystem::path& map_filename,
        const boost::filesystem::path& index_filename);

    /**
     * Initialize a new transaction database.
     */
    BCB_API void create();

    /**
     * You must call start() before using the database.
     */
    BCB_API void start();

    /**
     * Fetch block by height using the index table.
     */
    BCB_API block_result get(const size_t height) const;

    /**
     * Fetch block by hash using the hashtable.
     */
    BCB_API block_result get(const hash_digest& hash) const;

    /**
     * Store a block in the database.
     */
    BCB_API void store(const block_type& block);

    /**
     * Unlink all blocks upwards from (and including) from_height.
     */
    BCB_API void unlink(const size_t from_height);

    /**
     * Synchronise storage with disk so things are consistent.
     * Should be done at the end of every block write.
     */
    BCB_API void sync();

    /**
     * Latest block height in our chain. Returns block_database::null_height
     * if no blocks exist.
     */
    BCB_API size_t last_height() const;

private:
    typedef htdb_slab<hash_digest> map_type;

    /// Write position of tx.
    void write_position(const position_type position);

    /// Use intermediate records table to find blk position from height.
    position_type read_position(const index_type index) const;

    /// The hashtable used for looking up blocks by hash.
    mmfile map_file_;
    htdb_slab_header header_;
    slab_allocator allocator_;
    map_type map_;

    /// Table used for looking up blocks by height.
    /// Resolves to a position within the slab.
    mmfile index_file_;
    record_allocator index_;
};

} // namespace chain
} // namespace libbitcoin

#endif

