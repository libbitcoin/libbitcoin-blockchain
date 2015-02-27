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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_HPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_HPP

#include <bitcoin/blockchain/database/disk_array.hpp>
#include <bitcoin/blockchain/database/slab_allocator.hpp>

namespace libbitcoin {
    namespace chain {


/**
 * A hashtable mapping hashes to variable sized values (slabs).
 * Uses a combination of the disk_array and slab_allocator.
 *
 * The disk_array is basically a bucket list containing the start
 * value for the hashtable chain.
 *
 * The slab_allocator is used to create linked chains. A header
 * containing the hash of the item, and the next value is stored
 * with each slab.
 *
 *   [ HashType ]
 *   [ next:8   ]
 *   [ value... ]
 *
 * If we run allocator.sync() before the link() step then we ensure
 * data can be lost but the hashtable is never corrupted.
 * Instead we prefer speed and batch that operation. The user should
 * call allocator.sync() after a series of store() calls.
 */
template <typename HashType>
class htdb_slab
{
public:
    typedef std::function<void (uint8_t*)> write_function;

    htdb_slab(htdb_slab_header& header, slab_allocator& allocator);

    /**
     * Store a value. value_size is the requested size for the value.
     * The provided write() function must write exactly value_size bytes.
     * Returns the position of the inserted value in the slab_allocator.
     */
    position_type store(const HashType& key, write_function write,
        const size_t value_size);

    /**
     * Return the slab for a given hash.
     */
    slab_type get(const HashType& key) const;

    /**
     * Delete a key-value pair from the hashtable by unlinking the node.
     */
    bool unlink(const HashType& key);

private:
    /// What is the bucket given a hash.
    index_type bucket_index(const HashType& key) const;
    /// What is the slab start position for a chain.
    position_type read_bucket_value(const HashType& key) const;
    /// Link a new chain into the bucket header.
    void link(const HashType& key, const position_type begin);
    /// Release node from linked chain.
    template <typename ListItem>
    void release(const ListItem& item, const position_type previous);

    htdb_slab_header& header_;
    slab_allocator& allocator_;
};

    } // namespace chain
} // namespace libbitcoin

#include <bitcoin/blockchain/impl/htdb_slab.ipp>

#endif

