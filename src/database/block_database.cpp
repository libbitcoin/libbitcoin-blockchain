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
#include <bitcoin/blockchain/database/block_database.hpp>

#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/slab_allocator.hpp>

namespace libbitcoin {
namespace blockchain {

constexpr size_t number_buckets = 600000;
BC_CONSTEXPR size_t header_size = htdb_slab_header_fsize(number_buckets);
BC_CONSTEXPR size_t initial_map_file_size = header_size + min_slab_fsize;

BC_CONSTEXPR position_type allocator_offset = header_size;

// Record format:
// main:
//  [ header:80      ]
//  [ height:4       ]
//  [ number_txs:4   ]
// hashes:
//  [ [    ...     ] ]
//  [ [ tx_hash:32 ] ]
//  [ [    ...     ] ]

template <typename Iterator>
chain::block_header deserialize(const Iterator first)
{
    auto deserial = make_deserializer_unsafe(first);
    chain::block_header header(deserial);
    return header;
}

block_result::block_result(const slab_type slab)
  : slab_(slab)
{
}

block_result::operator bool() const
{
    return slab_ != nullptr;
}

chain::block_header block_result::header() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return deserialize(slab_);
}

size_t block_result::height() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return from_little_endian_unsafe<uint32_t>(slab_ + 80);
}

size_t block_result::transactions_size() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return from_little_endian_unsafe<uint32_t>(slab_ + 80 + 4);
}

hash_digest block_result::transaction_hash(size_t i) const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    BITCOIN_ASSERT(i < transactions_size());
    const uint8_t* first = slab_ + 80 + 4 + 4 + i * hash_size;
    auto deserial = make_deserializer_unsafe(first);
    return deserial.read_hash();
}

block_database::block_database(const boost::filesystem::path& map_filename,
    const boost::filesystem::path& index_filename)
  : map_file_(map_filename), 
    header_(map_file_, 0),
    allocator_(map_file_, allocator_offset),
    map_(header_, allocator_),
    index_file_(index_filename),
    index_(index_file_, 0, sizeof(position_type))
{
    BITCOIN_ASSERT(map_file_.data() != nullptr);
    BITCOIN_ASSERT(index_file_.data() != nullptr);
}

void block_database::create()
{
    map_file_.resize(initial_map_file_size);
    header_.create(number_buckets);
    allocator_.create();
    index_file_.resize(min_records_fsize);
    index_.create();
}

void block_database::start()
{
    header_.start();
    allocator_.start();
    index_.start();
}

block_result block_database::get(const size_t height) const
{
    if (height >= index_.count())
        return block_result(nullptr);

    const auto position = read_position(height);
    const auto slab = allocator_.get(position);
    return block_result(slab);
}

block_result block_database::get(const hash_digest& hash) const
{
    const auto slab = map_.get(hash);
    return block_result(slab);
}

void block_database::store(const chain::block& block)
{
    const uint32_t height = index_.count();
    const auto number_txs = block.transactions.size();
    const uint32_t number_txs32 = static_cast<uint32_t>(number_txs);

    // Write block data.
    const auto write = [&](uint8_t* data)
    {
        data_chunk header_data = block.header;
        auto serial = make_serializer(data);
        serial.write_data(header_data);
        serial.write_4_bytes(height);
        serial.write_4_bytes(number_txs32);

        for (const auto& tx: block.transactions)
        {
            const auto tx_hash = hash_transaction(tx);
            serial.write_hash(tx_hash);
        }
    };

    const auto key = block.header.hash();
    const auto value_size = 80 + 4 + 4 + number_txs * hash_size;
    const auto position = map_.store(key, write, value_size);

    // Write height -> position mapping.
    write_position(position);
}

void block_database::unlink(const size_t from_height)
{
    index_.count(from_height);
}

void block_database::sync()
{
    allocator_.sync();
    index_.sync();
}

size_t block_database::last_height() const
{
    if (index_.count() == 0)
        return null_height;

    return index_.count() - 1;
}

void block_database::write_position(const position_type position)
{
    const auto record = index_.allocate();
    const auto data = index_.get(record);
    auto serial = make_serializer(data);
    serial.write_8_bytes(position);
}

position_type block_database::read_position(const index_type index) const
{
    const auto record = index_.get(index);
    return from_little_endian_unsafe<position_type>(record);
}

} // namespace blockchain
} // namespace libbitcoin
