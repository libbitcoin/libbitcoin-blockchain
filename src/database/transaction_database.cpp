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

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/fsizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 10000;
constexpr size_t header_size = htdb_slab_header_fsize(number_buckets);
constexpr size_t initial_map_file_size = header_size + min_slab_fsize;

constexpr position_type alloc_offset = header_size;

template <typename Iterator>
transaction_type deserialize(const Iterator first)
{
    transaction_type tx;
    satoshi_load<Iterator, false>(first, nullptr, tx);
    return tx;
}

transaction_result::transaction_result(const slab_type slab)
  : slab_(slab)
{
}

transaction_result::operator bool() const
{
    return slab_ != nullptr;
}

size_t transaction_result::height() const
{
    BITCOIN_ASSERT(slab_);
    return from_little_endian<uint32_t>(slab_);
}

size_t transaction_result::index() const
{
    BITCOIN_ASSERT(slab_);
    return from_little_endian<uint32_t>(slab_ + 4);
}

transaction_type transaction_result::transaction() const
{
    BITCOIN_ASSERT(slab_);
    return deserialize(slab_ + 8);
}

transaction_database::transaction_database(
    const std::string& map_filename, const std::string& index_filename)
  : map_file_(map_filename), header_(map_file_, 0),
    allocator_(map_file_, alloc_offset), map_(header_, allocator_),
    index_file_(index_filename),
    index_(index_file_, 0, sizeof(position_type))
{
    BITCOIN_ASSERT(map_file_.data());
    BITCOIN_ASSERT(index_file_.data());
}

void transaction_database::initialize_new()
{
    map_file_.resize(initial_map_file_size);
    header_.initialize_new(number_buckets);
    allocator_.initialize_new();

    index_file_.resize(min_records_fsize);
    index_.initialize_new();
}

void transaction_database::start()
{
    header_.start();
    allocator_.start();
    index_.start();
}

transaction_result transaction_database::get(const index_type index) const
{
    if (index >= index_.size())
        return transaction_result(nullptr);
    const position_type position = read_position(index);
    const slab_type slab = allocator_.get(position);
    return transaction_result(slab);
}

transaction_result transaction_database::get(const hash_digest& hash) const
{
    const slab_type slab = map_.get(hash);
    return transaction_result(slab);
}

index_type transaction_database::store(
    const transaction_metainfo& info, const transaction_type& tx)
{
    // Write block data.
    const hash_digest key = hash_transaction(tx);
    const size_t value_size = 4 + 4 + satoshi_raw_size(tx);
    auto write = [&info, &tx](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_4_bytes(info.height);
        serial.write_4_bytes(info.index);
        satoshi_save(tx, serial.iterator());
    };
    const position_type position = map_.store(key, value_size, write);
    // Write index -> position mapping.
    return write_position(position);
}

void transaction_database::sync()
{
    allocator_.sync();
    index_.sync();
}

index_type transaction_database::write_position(const position_type position)
{
    const index_type index = index_.allocate();
    record_type record = index_.get(index);
    auto serial = make_serializer(record);
    serial.write_8_bytes(position);
    return index;
}

position_type transaction_database::read_position(const index_type index) const
{
    record_type record = index_.get(index);
    return from_little_endian<position_type>(record);
}

    } // namespace chain
} // namespace libbitcoin

