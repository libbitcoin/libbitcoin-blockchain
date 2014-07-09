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
#ifndef LIBBITCOIN_BLOCKCHAIN_RECORD_ALLOCATOR_HPP
#define LIBBITCOIN_BLOCKCHAIN_RECORD_ALLOCATOR_HPP

#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

typedef uint8_t* record_type;

/**
 * The record allocator represents a collection of fixed size chunks of
 * data referenced by an index. The file will be resized accordingly
 * and the total number of records updated so new chunks can be allocated.
 */
class record_allocator
{
public:
    typedef index_type accessor_type;

    BCB_API record_allocator(
        mmfile& file, position_type sector_start, size_t record_size);

    /**
      * Create record allocator.
      */
    BCB_API void initialize_new();

    /**
     * Prepare allocator for usage.
     */
    BCB_API void start();

    /**
     * Allocate a record.
     */
    BCB_API index_type allocate();

    /**
     * Synchronise record allocator to disk.
     */
    BCB_API void sync();

    /**
     * Return a slab.
     */
    BCB_API record_type get(index_type index);

private:
    void reserve();
    position_type position(index_type index);

    mmfile& file_;
    uint8_t* data_ = nullptr;
    const size_t record_size_ = 0;
    index_type end_ = 0;
};

    } // namespace chain
} // namespace libbitcoin

#endif

