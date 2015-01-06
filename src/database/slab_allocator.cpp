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

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

slab_allocator::slab_allocator(mmfile& file, position_type sector_start)
  : file_(file), sector_start_(sector_start)
{
}

void slab_allocator::initialize_new()
{
    static_assert(sizeof(end_) == 8, "Internal error");
    end_ = 8;
    sync();
}

void slab_allocator::start()
{
    BITCOIN_ASSERT(file_.size() >= 8);
    const slab_type data = file_.data() + sector_start_;
    end_ = from_little_endian_unsafe<uint64_t>(data);
}

position_type slab_allocator::allocate(size_t size)
{
    BITCOIN_ASSERT_MSG(end_ != 0, "slab_allocator::start() wasn't called.");
    reserve(size);
    position_type slab_position = end_;
    end_ += size;
    return slab_position;
}

void slab_allocator::sync()
{
    BITCOIN_ASSERT(end_ <= file_.size());
    auto serial = make_serializer(get(0));
    serial.write_8_bytes(end_);
}

slab_type slab_allocator::get(position_type position) const
{
#ifdef SLAB_DEBUG_ASSERTS
    // Disabled asserts to improve performance.
    BITCOIN_ASSERT_MSG(end_ != 0, "slab_allocator::start() wasn't called.");
    BITCOIN_ASSERT(position < end_);
    BITCOIN_ASSERT(sector_start_ + end_ <= file_.size());
#endif
    return file_.data() + sector_start_ + position;
}

void slab_allocator::reserve(size_t space_needed)
{
    // sector_start_ and end_ are uint64 but assigned to size_t.
    BITCOIN_ASSERT(end_ < bc::max_size_t);
    BITCOIN_ASSERT(sector_start_ < bc::max_size_t);

    // See comment in hsdb_shard::reserve()
    const size_t required_size = static_cast<size_t>(sector_start_) +
        static_cast<size_t>(end_) + space_needed;
    reserve_space(file_, required_size);
    BITCOIN_ASSERT(file_.size() >= required_size);
}

    } // namespace chain
} // namespace libbitcoin

