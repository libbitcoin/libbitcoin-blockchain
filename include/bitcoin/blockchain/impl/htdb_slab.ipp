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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_IPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_SLAB_IPP

#include <stdexcept>
#include <bitcoin/bitcoin.hpp>
#include "htdb_slab_list_item.ipp"
#include "remainder.ipp"

namespace libbitcoin {
namespace chain {

template <typename HashType>
htdb_slab<HashType>::htdb_slab(htdb_slab_header& header,
    slab_allocator& allocator)
  : header_(header), allocator_(allocator)
{
}

template <typename HashType>
position_type htdb_slab<HashType>::store(const HashType& key,
    write_function write, const size_t value_size)
{
    // Store current bucket value.
    const auto old_begin = read_bucket_value(key);
    htdb_slab_list_item<HashType> item(allocator_);
    const auto new_begin = item.create(key, value_size, old_begin);
    write(item.data());

    // Link record to header.
    link(key, new_begin);

    // Return position,
    return new_begin + item.value_begin;
}

template <typename HashType>
slab_type htdb_slab<HashType>::get(const HashType& key) const
{
    // Find start item...
    auto current = read_bucket_value(key);

    // For logging
    size_t index = 0;
    auto bucket = current;

    // Iterate through list...
    while (current != header_.empty)
    {
        const htdb_slab_list_item<HashType> item(allocator_, current);

        // Found.
        if (item.compare(key))
            return item.data();

        const auto previous = current;
        current = item.next_position();
        if (previous == current)
        {
            log_fatal(LOG_DATABASE)
                << "The slab database is corrupt getting ("
                << bucket << ")[" << index << "]";

            throw std::runtime_error("The database is corrupt.");
        }

        ++index;
    }

    // Not found.
    return nullptr;
}

template <typename HashType>
bool htdb_slab<HashType>::unlink(const HashType& key)
{
    // Find start item...
    const auto begin = read_bucket_value(key);
    const htdb_slab_list_item<HashType> begin_item(allocator_, begin);

    // If start item has the key then unlink from buckets.
    if (begin_item.compare(key))
    {
        link(key, begin_item.next_position());
        return true;
    }

    // For logging
    size_t index = 1;
    auto bucket = begin;

    // Continue on...
    auto previous = begin;
    auto current = begin_item.next_position();

    // Iterate through list...
    while (current != header_.empty)
    {
        const htdb_slab_list_item<HashType> item(allocator_, current);

        // Found, unlink current item from previous.
        if (item.compare(key))
        {
            release(item, previous);
            return true;
        }

        previous = current;
        current = item.next_position();
        if (previous == current)
        {
            log_fatal(LOG_DATABASE)
                << "The slab database is corrupt unlinking ("
                << bucket << ")[" << index << "]";

            throw std::runtime_error("The database is corrupt.");
        }

        ++index;
    }

    // Not found.
    return false;
}

template <typename HashType>
index_type htdb_slab<HashType>::bucket_index(const HashType& key) const
{
    const auto bucket = remainder(key, static_cast<uint32_t>(header_.size()));
    BITCOIN_ASSERT(bucket < header_.size());
    return bucket;
}

template <typename HashType>
position_type htdb_slab<HashType>::read_bucket_value(const HashType& key) const
{
    auto value = header_.read(bucket_index(key));
    static_assert(sizeof(value) == sizeof(position_type), "Invalid size");
    return value;
}

template <typename HashType>
void htdb_slab<HashType>::link(const HashType& key, const position_type begin)
{
    header_.write(bucket_index(key), begin);
}

template <typename HashType>
template <typename ListItem>
void htdb_slab<HashType>::release(const ListItem& item,
    const position_type previous)
{
    ListItem previous_item(allocator_, previous);
    previous_item.write_next_position(item.next_position());
}

} // namespace chain
} // namespace libbitcoin

#endif

