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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_RECORD_IPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_RECORD_IPP

#include <bitcoin/bitcoin.hpp>
#include "htdb_record_list_item.ipp"
#include "remainder.ipp"

namespace libbitcoin {
namespace chain {

template <typename HashType>
htdb_record<HashType>::htdb_record(
    htdb_record_header& header, record_allocator& allocator)
  : header_(header), allocator_(allocator)
{
}

template <typename HashType>
void htdb_record<HashType>::store(const HashType& key, write_function write)
{
    // Store current bucket value.
    const index_type old_begin = read_bucket_value(key);
    htdb_record_list_item<HashType> item(allocator_);
    const index_type new_begin =
        item.create(key, old_begin);
    write(item.data());
    // Link record to header.
    link(key, new_begin);
}

template <typename HashType>
record_type htdb_record<HashType>::get(const HashType& key) const
{
    // Find start item...
    index_type current = read_bucket_value(key);
    // Iterate through list...
    while (current != header_.empty)
    {
        const htdb_record_list_item<HashType> item(allocator_, current);
        // Found match, return data.
        if (item.compare(key))
            return item.data();
        current = item.next_index();
    }
    // Nothing found.
    return nullptr;
}

template <typename HashType>
bool htdb_record<HashType>::unlink(const HashType& key)
{
    // Find start item...
    const index_type begin = read_bucket_value(key);
    const htdb_record_list_item<HashType> begin_item(allocator_, begin);
    // If start item has the key then unlink from buckets.
    if (begin_item.compare(key))
    {
        link(key, begin_item.next_index());
        return true;
    }
    // Continue on...
    index_type previous = begin;
    index_type current = begin_item.next_index();
    // Iterate through list...
    while (current != header_.empty)
    {
        const htdb_record_list_item<HashType> item(allocator_, current);
        // Found match, unlink current item from previous.
        if (item.compare(key))
        {
            release(item, previous);
            return true;
        }
        previous = current;
        current = item.next_index();
    }
    // Nothing found.
    return false;
}

template <typename HashType>
index_type htdb_record<HashType>::bucket_index(const HashType& key) const
{
    const index_type bucket = remainder(key, header_.size());
    BITCOIN_ASSERT(bucket < header_.size());
    return bucket;
}

template <typename HashType>
index_type htdb_record<HashType>::read_bucket_value(
    const HashType& key) const
{
    auto value = header_.read(bucket_index(key));
    BITCOIN_ASSERT(sizeof(value) == sizeof(index_type));
    return value;
}

template <typename HashType>
void htdb_record<HashType>::link(
    const HashType& key, const index_type begin)
{
    header_.write(bucket_index(key), begin);
}

template <typename HashType>
template <typename ListItem>
void htdb_record<HashType>::release(
    const ListItem& item, const position_type previous)
{
    ListItem previous_item(allocator_, previous);
    previous_item.write_next_index(item.next_index());
}

} // namespace chain
} // namespace libbitcoin

#endif

