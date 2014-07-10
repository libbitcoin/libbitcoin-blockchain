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
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

typedef disk_array<index_type, position_type> htdb_slab_header;

/**
 * A hashtable mapping hashes to variable sized values.
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
     */
    void store(const HashType& key,
        const size_t value_size, write_function write);

    /**
     * Return the slab for a given hash.
     */
    const slab_type get(const HashType& key) const;

private:
    /// What is the bucket given a hash.
    index_type bucket_index(const HashType& key) const;
    /// What is the slab start position for a chain.
    position_type read_bucket_value(const HashType& key) const;
    /// Link a new chain into the bucket header.
    void link(const HashType& key, const position_type begin);

    htdb_slab_header& header_;
    slab_allocator& allocator_;
};

    } // namespace chain
} // namespace libbitcoin

#include <bitcoin/blockchain/impl/htdb_slab.ipp>

#endif

