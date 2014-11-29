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
#ifndef LIBBITCOIN_BLOCKCHAIN_DISK_ARRAY_IPP
#define LIBBITCOIN_BLOCKCHAIN_DISK_ARRAY_IPP

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
    namespace chain {

template <typename IndexType, typename ValueType>
disk_array<IndexType, ValueType>::disk_array(
    mmfile& file, position_type sector_start)
  : file_(file), sector_start_(sector_start)
{
    static_assert(std::is_unsigned<ValueType>::value,
        "disk_array only works with unsigned types");
}

template <typename IndexType, typename ValueType>
void disk_array<IndexType, ValueType>::initialize_new(
    IndexType size)
{
    auto serial = make_serializer(data(0));
    serial.write_little_endian(size);
    for (IndexType i = 0; i < size; ++i)
        serial.write_little_endian(empty);
}

template <typename IndexType, typename ValueType>
void disk_array<IndexType, ValueType>::start()
{
    const uint8_t* begin = data(0);
    const uint8_t* end = data(sizeof(IndexType));
    auto deserial = make_deserializer(begin, end);
    size_ = deserial.read_little_endian<IndexType>();
}

template <typename IndexType, typename ValueType>
ValueType disk_array<IndexType, ValueType>::read(
    IndexType index) const
{
    BITCOIN_ASSERT_MSG(size_ != 0, "disk_array::start() wasn't called.");
    BITCOIN_ASSERT(index < size_);
    // Find our item.
    const position_type position = item_position(index);
    const uint8_t* begin = data(position);
    // Deserialize value.
    return from_little_endian_unsafe<ValueType>(begin);
}

template <typename IndexType, typename ValueType>
void disk_array<IndexType, ValueType>::write(
    IndexType index, ValueType value)
{
    BITCOIN_ASSERT_MSG(size_ != 0, "disk_array::start() wasn't called.");
    BITCOIN_ASSERT(index < size_);
    // Find our item.
    const position_type position = item_position(index);
    // Write the value.
    auto serial = make_serializer(data(position));
    serial.write_little_endian(value);
}

template <typename IndexType, typename ValueType>
IndexType disk_array<IndexType, ValueType>::size() const
{
    return size_;
}

template <typename IndexType, typename ValueType>
position_type disk_array<IndexType, ValueType>::item_position(
    IndexType index) const
{
    return sizeof(IndexType) + index * sizeof(ValueType);
}

template <typename IndexType, typename ValueType>
uint8_t* disk_array<IndexType, ValueType>::data(position_type position) const
{
    BITCOIN_ASSERT(sector_start_ + position <= file_.size());
    return file_.data() + sector_start_ + position;
}

    } // namespace chain
} // namespace libbitcoin

#endif

