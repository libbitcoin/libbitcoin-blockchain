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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_LIST_ITEM_IPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_LIST_ITEM_IPP

namespace libbitcoin {
    namespace chain {

/**
 * Item for htdb_slab. A chained list with the key included.
 *
 * Stores the key, next position and user data.
 * With the starting item, we can iterate until the end using the
 * next_position() method.
 */
template <typename HashType>
class htdb_slab_list_item
{
public:
    htdb_slab_list_item(
        slab_allocator& allocator, const position_type position=0);

    position_type initialize_new(
        const HashType& key, const size_t value_size,
        const position_type next);

    // Does this match?
    bool compare(const HashType& key) const;
    // The actual user data.
    slab_type data() const;
    // Position of next item in the chained list.
    position_type next_position() const;

private:
    static constexpr size_t hash_size = std::tuple_size<HashType>::value;

    slab_allocator& allocator_;
    slab_type raw_data_;
};

template <typename HashType>
htdb_slab_list_item<HashType>::htdb_slab_list_item(
    slab_allocator& allocator, const position_type position)
  : allocator_(allocator), raw_data_(allocator_.get(position))
{
}

template <typename HashType>
position_type htdb_slab_list_item<HashType>::initialize_new(
    const HashType& key, const size_t value_size, const position_type next)
{
    const position_type info_size = key.size() + 8;
    BITCOIN_ASSERT(sizeof(position_type) == 8);
    // Create new slab.
    //   [ HashType ]
    //   [ next:8   ]
    //   [ value... ]
    const size_t slab_size = info_size + value_size;
    const position_type position = allocator_.allocate(slab_size);
    raw_data_ = allocator_.get(position);
    // Write to slab.
    auto serial = make_serializer(raw_data_);
    serial.write_data(key);
    serial.write_8_bytes(next);
    return position;
}

template <typename HashType>
bool htdb_slab_list_item<HashType>::compare(const HashType& key) const
{
    // Key data is at the start.
    const uint8_t* key_data = raw_data_;
    return std::equal(key.begin(), key.end(), key_data);
}

template <typename HashType>
slab_type htdb_slab_list_item<HashType>::data() const
{
    // Value data is at the end.
    constexpr position_type value_begin = hash_size + 8;
    return raw_data_ + value_begin;
}

template <typename HashType>
position_type htdb_slab_list_item<HashType>::next_position() const
{
    // Next position is after key data.
    static_assert(sizeof(position_type) == 8, "Internal error");
    constexpr position_type next_begin = hash_size;
    const slab_type next_data = raw_data_ + next_begin;
    // Read the next position.
    return from_little_endian<position_type>(next_data);
}

    } // namespace chain
} // namespace libbitcoin

#endif

