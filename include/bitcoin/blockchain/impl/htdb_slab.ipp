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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_IPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_IPP

#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

template <typename HashType>
htdb_slab<HashType>::htdb_slab(
    htdb_slab_header& header, slab_allocator& allocator)
  : header_(header), allocator_(allocator)
{
}

template <typename HashType>
void htdb_slab<HashType>::store(const HashType& key,
    const size_t value_size, write_function write)
{
    const position_type info_size = key.size() + 8;
    // Store current bucket value.
    const position_type old_begin = read_bucket_value(key);
    BITCOIN_ASSERT(sizeof(position_type) == 8);
    // Create new slab.
    //   [ HashType ]
    //   [ next:8   ]
    //   [ value... ]
    const size_t slab_size = info_size + value_size;
    position_type slab_position = allocator_.allocate(slab_size);
    // Write to slab.
    uint8_t* slab = allocator_.get(slab_position);
    auto serial = make_serializer(slab);
    serial.write_data(key);
    serial.write_8_bytes(old_begin);
    write(serial.iterator());
    // Link record to header.
    link(key, slab_position);
}

template <typename HashType>
const slab_type htdb_slab<HashType>::get(const HashType& key) const
{
    const position_type value_position = key.size() + 8;
    // Find start item...
    position_type next = read_bucket_value(key);
    while (next != header_.empty)
    {
        uint8_t* slab = allocator_.get(next);
        // We have found our matching item.
        if (std::equal(key.begin(), key.end(), slab))
            return slab + value_position;
        // Continue to next slab...
        uint8_t* next_data = slab + key.size();
        auto deserial = make_deserializer(next_data, next_data + 8);
        next = deserial.read_8_bytes();
    }
    // Nothing found.
    return nullptr;
}

template <typename HashType>
index_type htdb_slab<HashType>::bucket_index(const HashType& key) const
{
    const index_type bucket = remainder(key, header_.size());
    BITCOIN_ASSERT(bucket < header_.size());
    return bucket;
}

template <typename HashType>
position_type htdb_slab<HashType>::read_bucket_value(
    const HashType& key) const
{
    auto value = header_.read(bucket_index(key));
    BITCOIN_ASSERT(sizeof(value) == sizeof(position_type));
    return value;
}

template <typename HashType>
void htdb_slab<HashType>::link(
    const HashType& key, const position_type begin)
{
    header_.write(bucket_index(key), begin);
}

    } // namespace chain
} // namespace libbitcoin

#endif

