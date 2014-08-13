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
#include <bitcoin/blockchain/database/transaction_database.hpp>

#include <bitcoin/satoshi_serialize.hpp>
#include <bitcoin/transaction.hpp>
#include <bitcoin/blockchain/database/sizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 10000;
constexpr size_t header_size = htdb_slab_header_size(number_buckets);
constexpr size_t initial_map_file_size = header_size + min_slab_size;

constexpr position_type alloc_offset = header_size;

transaction_database::transaction_database(
    const std::string& map_filename, const std::string& records_filename)
  : map_file_(map_filename), header_(map_file_, 0),
    allocator_(map_file_, alloc_offset), map_(header_, allocator_),
    records_file_(records_filename),
    records_(records_file_, 0, sizeof(position_type))
{
    BITCOIN_ASSERT(map_file_.data());
    BITCOIN_ASSERT(records_file_.data());
}

void transaction_database::initialize_new()
{
    map_file_.resize(initial_map_file_size);
    header_.initialize_new(number_buckets);
    allocator_.initialize_new();

    records_file_.resize(min_records_size);
    records_.initialize_new();
}

void transaction_database::start()
{
    header_.start();
    allocator_.start();
    records_.start();
}

template <typename Iterator>
transaction_type deserialize(const Iterator first)
{
    transaction_type tx;
    satoshi_load<Iterator, false>(first, nullptr, tx);
    return tx;
}

void transaction_database::fetch(const index_type index,
    fetch_handler handle_fetch) const
{
    if (index >= records_.end())
    {
        handle_fetch(error::not_found, transaction_type());
        return;
    }
    const position_type position = read_position(index);
    const slab_type slab = allocator_.get(position);
    const transaction_type tx = deserialize(slab);
    // Need some protection against user error here...
    // Might need a separate record_allocator to look actual position before.
    handle_fetch(std::error_code(), tx);
}

void transaction_database::fetch(const hash_digest& hash,
    fetch_handler handle_fetch) const
{
    const slab_type slab = map_.get(hash);
    if (!slab)
    {
        handle_fetch(error::not_found, transaction_type());
        return;
    }
    const transaction_type tx = deserialize(slab);
    handle_fetch(std::error_code(), tx);
}

index_type transaction_database::store(const transaction_type& tx)
{
    const hash_digest key = hash_transaction(tx);
    const size_t value_size = satoshi_raw_size(tx);
    auto write = [&tx](uint8_t* data)
    {
        satoshi_save(tx, data);
    };
    position_type position = map_.store(key, value_size, write);
    return write_position(position);
}

void transaction_database::sync()
{
    allocator_.sync();
}

index_type transaction_database::write_position(const position_type position)
{
    const index_type index = records_.allocate();
    record_type record = records_.get(index);
    auto serial = make_serializer(record);
    serial.write_8_bytes(position);
    return index;
}

position_type transaction_database::read_position(const index_type index) const
{
    record_type record = records_.get(index);
    return from_little_endian<position_type>(record);
}

    } // namespace chain
} // namespace libbitcoin

