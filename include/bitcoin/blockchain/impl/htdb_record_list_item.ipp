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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_RECORD_LIST_ITEM_IPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_RECORD_LIST_ITEM_IPP

namespace libbitcoin {
namespace chain {

/**
 * Item for htdb_record. A chained list with the key included.
 *
 * Stores the key, next index and user data.
 * With the starting item, we can iterate until the end using the
 * next_index() method.
 */
template <typename HashType>
class htdb_record_list_item
{
public:
    htdb_record_list_item(record_allocator& allocator,
        const index_type index=0);

    index_type create(const HashType& key, const index_type next);

    // Does this match?
    bool compare(const HashType& key) const;

    // The actual user data.
    record_type data() const;

    // Position of next item in the chained list.
    index_type next_index() const;

    // Write a new next index.
    void write_next_index(index_type next);

private:
    uint8_t* raw_data(position_type offset) const;
    uint8_t* raw_next_data() const;

    record_allocator& allocator_;
    index_type index_;
};

template <typename HashType>
htdb_record_list_item<HashType>::htdb_record_list_item(
    record_allocator& allocator, const index_type index)
  : allocator_(allocator), index_(index)
{
}

template <typename HashType>
index_type htdb_record_list_item<HashType>::create(
    const HashType& key, const index_type next)
{
    // Create new record.
    //   [ HashType ]
    //   [ next:4   ]
    //   [ value... ]
    index_ = allocator_.allocate();
    record_type data = allocator_.get(index_);

    // Write record.
    auto serial = make_serializer(data);
    serial.write_data(key);

    // MUST BE ATOMIC ???
    serial.write_4_bytes(next);
    return index_;
}

template <typename HashType>
bool htdb_record_list_item<HashType>::compare(const HashType& key) const
{
    // Key data is at the start.
    const auto key_data = raw_data(0);
    return std::equal(key.begin(), key.end(), key_data);
}

template <typename HashType>
record_type htdb_record_list_item<HashType>::data() const
{
    // Value data is at the end.
    BC_CONSTEXPR size_t hash_size = std::tuple_size<HashType>::value;
    BC_CONSTEXPR position_type value_begin = hash_size + 4;
    return raw_data(value_begin);
}

template <typename HashType>
index_type htdb_record_list_item<HashType>::next_index() const
{
    const auto next_data = raw_next_data();
    return from_little_endian_unsafe<index_type>(next_data);
}

template <typename HashType>
void htdb_record_list_item<HashType>::write_next_index(index_type next)
{
    const auto next_data = raw_next_data();
    auto serial = make_serializer(next_data);

    // MUST BE ATOMIC ???
    serial.write_4_bytes(next);
}

template <typename HashType>
uint8_t* htdb_record_list_item<HashType>::raw_data(position_type offset) const
{
    return allocator_.get(index_) + offset;
}

template <typename HashType>
uint8_t* htdb_record_list_item<HashType>::raw_next_data() const
{
    // Next position is after key data.
    BITCOIN_ASSERT(sizeof(index_type) == 4);
    BC_CONSTEXPR size_t hash_size = std::tuple_size<HashType>::value;
    BC_CONSTEXPR position_type next_begin = hash_size;
    return raw_data(next_begin);
}

} // namespace chain
} // namespace libbitcoin

#endif

