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
#include <bitcoin/blockchain/database/linked_records.hpp>

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace chain {

linked_records::linked_records(record_allocator& allocator)
  : allocator_(allocator)
{
}

index_type linked_records::create()
{
    // Insert new record with empty next value.
    return insert(empty);
}

index_type linked_records::insert(index_type next)
{
    static_assert(sizeof(index_type) == sizeof(uint32_t),
        "index_type incorrect size");

    // Create new record.
    auto record = allocator_.allocate();
    auto data = allocator_.get(record);

    // Write next value at first 4 bytes of record.
    auto serial = make_serializer(data);

    // MUST BE ATOMIC ???
    serial.write_4_bytes(next);
    return record;
}

index_type linked_records::next(index_type index) const
{
    const auto data = allocator_.get(index);
    return from_little_endian_unsafe<index_type>(data);
}

record_type linked_records::get(index_type index) const
{
    return allocator_.get(index) + sizeof(index_type);
}

} // namespace chain
} // namespace libbitcoin

