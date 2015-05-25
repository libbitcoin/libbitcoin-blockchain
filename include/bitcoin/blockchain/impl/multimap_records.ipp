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
#ifndef LIBBITCOIN_BLOCKCHAIN_MULTIMAP_RECORDS_IPP
#define LIBBITCOIN_BLOCKCHAIN_MULTIMAP_RECORDS_IPP

namespace libbitcoin {
namespace chain {

template <typename HashType>
multimap_records<HashType>::multimap_records(
    htdb_type& map, linked_records& linked_rows)
  : map_(map), linked_rows_(linked_rows)
{
}

template <typename HashType>
index_type multimap_records<HashType>::lookup(const HashType& key) const
{
    const record_type start_info = map_.get(key);
    if (!start_info)
        return linked_rows_.empty;
    const index_type first = from_little_endian_unsafe<index_type>(start_info);
    return first;
}

template <typename HashType>
void multimap_records<HashType>::add_row(
    const HashType& key, write_function write)
{
    record_type start_info = map_.get(key);
    if (!start_info)
    {
        create_new(key, write);
        return;
    }
    add_to_list(start_info, write);
}

template <typename HashType>
void multimap_records<HashType>::delete_last_row(const HashType& key)
{
    record_type start_info = map_.get(key);
    BITCOIN_ASSERT(start_info != nullptr);
    const index_type old_begin = 
        from_little_endian_unsafe<index_type>(start_info);
    BITCOIN_ASSERT(old_begin != linked_rows_.empty);
    const index_type new_begin = linked_rows_.next(old_begin);
    if (new_begin == linked_rows_.empty)
    {
        DEBUG_ONLY(bool success =) map_.unlink(key);
        BITCOIN_ASSERT(success);
        return;
    }
    auto serial = make_serializer(start_info);
    serial.write_4_bytes(new_begin);
}

template <typename HashType>
void multimap_records<HashType>::add_to_list(
    record_type start_info, write_function write)
{
    const index_type old_begin = 
        from_little_endian_unsafe<index_type>(start_info);
    const index_type new_begin = linked_rows_.insert(old_begin);
    record_type record = linked_rows_.get(new_begin);
    write(record);
    auto serial = make_serializer(start_info);
    serial.write_4_bytes(new_begin);
}

template <typename HashType>
void multimap_records<HashType>::create_new(
    const HashType& key, write_function write)
{
    const index_type first = linked_rows_.create();
    record_type record = linked_rows_.get(first);
    write(record);
    auto write_start_info = [first](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_4_bytes(first);
    };
    map_.store(key, write_start_info);
}

} // namespace chain
} // namespace libbitcoin

#endif


