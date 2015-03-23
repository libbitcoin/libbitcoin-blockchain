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

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/disk_array.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>

namespace libbitcoin {
    namespace chain {

typedef uint8_t* record_type;
typedef disk_array<index_type, index_type> htdb_record_header;

BC_CONSTEXPR size_t min_records_fsize = sizeof(index_type);
BC_CONSTFUNC size_t htdb_record_header_fsize(size_t buckets)
{
    return sizeof(index_type) + min_records_fsize * buckets;
}

/**
 * The record allocator represents a collection of fixed size chunks of
 * data referenced by an index. The file will be resized accordingly
 * and the total number of records updated so new chunks can be allocated.
 */
class record_allocator
{
public:
    BCB_API record_allocator(
        mmfile& file, position_type sector_start, size_t record_size);

    /**
      * Create record allocator.
      */
    BCB_API void create();

    /**
     * Prepare allocator for usage.
     */
    BCB_API void start();

    /**
     * Allocate a record and return its logical index.
     * Call sync() after writing the record.
     */
    BCB_API index_type allocate(/* size_t records=1 */);

    /**
     * Synchronise to disk.
     */
    BCB_API void sync();

    /**
     * Return a record from its logical index.
     */
    BCB_API record_type get(index_type record) const;

    /**
     * The number of records in this container.
     */
    BCB_API index_type count() const;

    /**
     * Change the number of records of this container.
     */
    BCB_API void count(const index_type records);

private:
    /// File data access, by byte-wise position relative to start.
    uint8_t* data(const position_type position) const;

    /// Ensure bytes for a new record are available.
    void reserve(size_t count);

    /// The record index of a disk position.
    ///index_type position_to_record(position_type position) const;

    /// The disk position of a record index.
    position_type record_to_position(index_type record) const;

    /// Read the count of the records from the file.
    void read_count();

    /// Write the count of the records from the file.
    void write_count();

    mmfile& file_;
    position_type start_;
    index_type count_;
    const size_t record_size_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

