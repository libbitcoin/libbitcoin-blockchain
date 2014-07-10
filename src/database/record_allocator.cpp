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
#include <bitcoin/blockchain/database/record_allocator.hpp>

#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

record_allocator::record_allocator(
    mmfile& file, position_type sector_start, size_t record_size)
  : file_(file), sector_start_(sector_start), record_size_(record_size)
{
}

void record_allocator::initialize_new()
{
    BITCOIN_ASSERT(sizeof(end_) == 4);
    end_ = 0;
    sync();
}

void record_allocator::start()
{
    BITCOIN_ASSERT(file_.size() >= 4);
    auto deserial = make_deserializer(data(0), data(4));
    end_ = deserial.read_4_bytes();
}

index_type record_allocator::allocate()
{
    reserve();
    const index_type record_index = end_;
    ++end_;
    BITCOIN_ASSERT(record_position(end_) <= file_.size());
    return record_index;
}

void record_allocator::sync()
{
    BITCOIN_ASSERT(record_position(end_) <= file_.size());
    auto serial = make_serializer(data(0));
    serial.write_4_bytes(end_);
}

record_type record_allocator::get(index_type index)
{
    BITCOIN_ASSERT(index < end_);
    BITCOIN_ASSERT(record_position(end_) <= file_.size());
    return data(record_position(index));
}

void record_allocator::reserve()
{
    const size_t required_size =
        sector_start_ + 4 + (end_ + 1) * record_size_;
    reserve_space(file_, required_size);
    BITCOIN_ASSERT(file_.size() >= required_size);
}

uint8_t* record_allocator::data(position_type position)
{
    BITCOIN_ASSERT(sector_start_ + position <= file_.size());
    return file_.data() + sector_start_ + position;
}

position_type record_allocator::record_position(index_type index)
{
    return 4 + index * record_size_;
}

    } // namespace chain
} // namespace libbitcoin

