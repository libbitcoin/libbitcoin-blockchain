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
#ifndef LIBBITCOIN_BLOCKCHAIN_SLAB_ALLOCATOR_HPP
#define LIBBITCOIN_BLOCKCHAIN_SLAB_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/disk_array.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>

namespace libbitcoin {
namespace blockchain {

typedef uint8_t* slab_type;
typedef disk_array<index_type, position_type> htdb_slab_header;

BC_CONSTEXPR size_t min_slab_fsize = sizeof(position_type);
BC_CONSTFUNC size_t htdb_slab_header_fsize(size_t buckets)
{
    return sizeof(position_type) + min_slab_fsize * buckets;
}

/**
 * The slab allocator represents a growing collection of various sized
 * slabs of data on disk. It will resize the file accordingly and keep
 * track of the current end pointer so new slabs can be allocated.
 */
class slab_allocator
{
public:
    BCB_API slab_allocator(mmfile& file, position_type sector_start);

    /**
      * Create slab allocator.
      */
    BCB_API void create();

    /**
     * Prepare allocator for usage.
     */
    BCB_API void start();

    /**
     * Allocate a slab.
     * Call sync() after writing the record.
     */
    BCB_API position_type allocate(size_t bytes_needed);

    /**
     * Synchronise slab allocator to disk.
     */
    BCB_API void sync();

    /**
     * Return a slab from its byte-wise position relative to start.
     */
    BCB_API slab_type get(position_type position) const;

private:
    /// File data access, by byte-wise position relative to start.
    uint8_t* data(const position_type position) const;

    /// Ensure bytes for a new record are available.
    void reserve(size_t bytes_needed);

    /// Read the size of the data from the file.
    void read_size();

    /// Write the size of the data from the file.
    void write_size();

    mmfile& file_;
    position_type start_;
    position_type size_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
