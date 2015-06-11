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

#include <stdexcept>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>

namespace libbitcoin {
namespace chain {

record_allocator::record_allocator(mmfile& file, position_type sector_start,
    size_t record_size)
  : file_(file), start_(sector_start), count_(0), record_size_(record_size)
{
}

void record_allocator::create()
{
    // count_ = 0;
    sync();
}

void record_allocator::start()
{
    read_count();
}

void record_allocator::sync()
{
    write_count();
}

index_type record_allocator::allocate(/* size_t records=1 */)
{
    constexpr size_t records = 1;
    auto record_index = count_;
    reserve(records);
    return record_index;
}

// logical record access
record_type record_allocator::get(index_type record) const
{
    return data(record_to_position(record));
}

// File data access, by byte-wise position relative to file.
uint8_t* record_allocator::data(const position_type position) const
{
    // For some reason calling data(start_ + position) is way slower.
    BITCOIN_ASSERT(start_ + position <= file_.size());
    return file_.data() + start_ + position;
}

void record_allocator::reserve(size_t count)
{
    // See comment in hsdb_shard::reserve()
    const size_t required_size = start_ + record_to_position(count_ + count);
    const auto reserved = file_.reserve(required_size);

    // There is no way to recover from this.
    if (!reserved)
        throw std::runtime_error(
            "The file could not be resized, disk space may be low.");

    count_ += count;
}

// Read the count value from the first chunk of the file.
void record_allocator::read_count()
{
    BITCOIN_ASSERT(file_.size() >= sizeof(count_));
    count_ = from_little_endian_unsafe<index_type>(data(0));
}

// Write the count value to the first chunk of the file.
void record_allocator::write_count()
{
    BITCOIN_ASSERT(file_.size() >= sizeof(count_));
    auto serial = make_serializer(data(0));
    serial.write_little_endian(count_);
}

// A record allocator is a slab allocator where the record size is fixed.
// These methods convert from logical record position to byte-wise position
// relative to start.

//index_type record_allocator::position_to_record(position_type position) const
//{
//    return (position - sizeof(index_type)) / record_size_;
//}

position_type record_allocator::record_to_position(index_type record) const
{
    // The position is relative to start, so we must add the size prefix.
    return sizeof(index_type) + record * record_size_;
}

index_type record_allocator::count() const
{
    return count_;
}

void record_allocator::count(const index_type records)
{
    BITCOIN_ASSERT(records <= count_);
    count_ = records;
}

} // namespace chain
} // namespace libbitcoin

