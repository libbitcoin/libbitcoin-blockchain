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
#include <bitcoin/blockchain/database/slab_allocator.hpp>

#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>

namespace libbitcoin {
    namespace chain {

slab_allocator::slab_allocator(mmfile& file, position_type sector_start)
  : file_(file)
{
    data_ = file_.data() + sector_start;
}

void slab_allocator::initialize_new()
{
    end_ = 8;
    sync();
}

void slab_allocator::start()
{
    BITCOIN_ASSERT(file_.size() > 8);
    auto deserial = make_deserializer(data_, data_ + 8);
    end_ = deserial.read_8_bytes();
}

position_type slab_allocator::allocate(size_t size)
{
    BITCOIN_ASSERT_MSG(end_ != 0, "slab_allocator::start() wasn't called.");
    reserve(size);
    BITCOIN_ASSERT(file_.size() >= end_ + size);
    position_type slab_position = end_;
    end_ += size;
    return slab_position;
}

void slab_allocator::sync()
{
    BITCOIN_ASSERT(file_.size() > end_);
    auto serial = make_serializer(data_);
    serial.write_8_bytes(end_);
}

slab_type slab_allocator::get(position_type position)
{
    BITCOIN_ASSERT_MSG(end_ != 0, "slab_allocator::start() wasn't called.");
    BITCOIN_ASSERT(position < file_.size());
    return data_ + position;
}

const slab_type slab_allocator::get(position_type position) const
{
    BITCOIN_ASSERT_MSG(end_ != 0, "slab_allocator::start() wasn't called.");
    BITCOIN_ASSERT(position < file_.size());
    return data_ + position;
}

void slab_allocator::reserve(size_t space_needed)
{
    const size_t required_size = end_ + space_needed;
    if (required_size <= file_.size())
        return;
    // Grow file by 1.5x
    const size_t new_size = required_size * 3 / 2;
    // Only ever grow file. Never shrink it!
    BITCOIN_ASSERT(new_size > file_.size());
    bool success = file_.resize(new_size);
    BITCOIN_ASSERT(success);
}

    } // namespace chain
} // namespace libbitcoin

