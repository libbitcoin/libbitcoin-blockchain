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
#include <bitcoin/blockchain/database/slab_allocator.hpp>

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>

namespace libbitcoin {
namespace blockchain {

slab_allocator::slab_allocator(mmfile& file, position_type sector_start)
  : file_(file), start_(sector_start), size_(0)
{
}

// This writes the byte size of the allocated space to the file.
void slab_allocator::create()
{
    size_ = sizeof(position_type);
    sync();
}

void slab_allocator::start()
{
    read_size();
}

void slab_allocator::sync()
{
    write_size();
}

position_type slab_allocator::allocate(size_t bytes_needed)
{
    BITCOIN_ASSERT_MSG(size_ > 0, "slab_allocator::start() wasn't called.");
    const auto slab_position = size_;
    reserve(bytes_needed);
    return slab_position;
}

// logical slab access
slab_type slab_allocator::get(position_type position) const
{
    BITCOIN_ASSERT_MSG(size_ > 0, "slab_allocator::start() wasn't called.");
    BITCOIN_ASSERT(position < size_);
    return data(position);
}

// File data access, by byte-wise position relative to start.
uint8_t* slab_allocator::data(const position_type position) const
{
    BITCOIN_ASSERT(start_ + position <= file_.size());
    return file_.data() + start_ + position;
}

void slab_allocator::reserve(size_t bytes_needed)
{
    // See comment in hsdb_shard::reserve()
    const size_t required_size = start_ + size_ + bytes_needed;
    DEBUG_ONLY(const auto success =) file_.reserve(required_size);
    BITCOIN_ASSERT(success);
    size_ += bytes_needed;
}

// Read the size value from the first chunk of the file.
// Since the write is not atomic this must be read before write is enabled.
void slab_allocator::read_size()
{
    BITCOIN_ASSERT(file_.size() >= sizeof(size_));
    size_ = from_little_endian_unsafe<position_type>(data(0));
}

// Write the size value to the first chunk of the file.
// This write is not atomic and therefore assumes there are no readers.
void slab_allocator::write_size()
{
    BITCOIN_ASSERT(size_ <= file_.size());
    auto serial = make_serializer(data(0));
    serial.write_little_endian(size_);
}

} // namespace blockchain
} // namespace libbitcoin
