/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
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
namespace blockchain {

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
    static BC_CONSTEXPR size_t hash_size = std::tuple_size<HashType>::value;
    static BC_CONSTEXPR position_type value_begin = hash_size + 8;

    htdb_slab_list_item(slab_allocator& allocator,
        const position_type position=0);

    position_type create(const HashType& key, const size_t value_size,
        const position_type next);

    // Does this match?
    bool compare(const HashType& key) const;

    // The actual user data.
    slab_type data() const;

    // Position of next item in the chained list.
    position_type next_position() const;

    // Write a new next position.
    void write_next_position(position_type next);

private:
    uint8_t* raw_next_data() const;

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
position_type htdb_slab_list_item<HashType>::create(const HashType& key,
    const size_t value_size, const position_type next)
{
    const position_type info_size = key.size() + 8;
    BITCOIN_ASSERT(sizeof(position_type) == 8);

    // Create new slab.
    //   [ HashType ]
    //   [ next:8   ]
    //   [ value... ]
    const size_t slab_size = info_size + value_size;
    const position_type slab = allocator_.allocate(slab_size);
    raw_data_ = allocator_.get(slab);

    // Write to slab.
    auto serial = make_serializer(raw_data_);
    serial.write_data(key);

    // MUST BE ATOMIC ???
    serial.write_8_bytes_little_endian(next);
    return slab;
}

template <typename HashType>
bool htdb_slab_list_item<HashType>::compare(const HashType& key) const
{
    // Key data is at the start.
    const auto key_data = raw_data_;
    return std::equal(key.begin(), key.end(), key_data);
}

template <typename HashType>
slab_type htdb_slab_list_item<HashType>::data() const
{
    // Value data is at the end.
    return raw_data_ + value_begin;
}

template <typename HashType>
position_type htdb_slab_list_item<HashType>::next_position() const
{
    const auto next_data = raw_next_data();
    return from_little_endian_unsafe<position_type>(next_data);
}

template <typename HashType>
void htdb_slab_list_item<HashType>::write_next_position(position_type next)
{
    auto next_data = raw_next_data();
    auto serial = make_serializer(next_data);

    // MUST BE ATOMIC ???
    serial.write_8_bytes_little_endian(next);
}

template <typename HashType>
uint8_t* htdb_slab_list_item<HashType>::raw_next_data() const
{
    // Next position is after key data.
    static_assert(sizeof(position_type) == 8, "Internal error");
    BC_CONSTEXPR position_type next_begin = hash_size;
    return raw_data_ + next_begin;
}

} // namespace blockchain
} // namespace libbitcoin

#endif
