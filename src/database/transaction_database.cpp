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
#include <bitcoin/blockchain/database/transaction_database.hpp>

#include <boost/filesystem.hpp>
//#include <boost/iostreams/stream.hpp>
#include <bitcoin/bitcoin.hpp>
//#include <bitcoin/blockchain/pointer_array_source.hpp>

namespace libbitcoin {
namespace blockchain {

constexpr size_t number_buckets = 100000000;
BC_CONSTEXPR size_t header_size = htdb_slab_header_fsize(number_buckets);
BC_CONSTEXPR size_t initial_map_file_size = header_size + min_slab_fsize;

BC_CONSTEXPR position_type alloc_offset = header_size;

//chain::transaction deserialize_tx(const slab_type begin, uint64_t length)
//{
//    boost::iostreams::stream<byte_pointer_array_source> istream(begin, length);
//    istream.exceptions(std::ios_base::failbit);
//    chain::transaction tx;
//    tx.from_data(istream);
//
////    if (!istream)
////        throw end_of_stream();
//
//    return tx;
//}

template <typename Iterator>
chain::transaction deserialize_tx(const Iterator first)
{
    chain::transaction tx;
    auto deserial = make_deserializer_unsafe(first);
    tx.from_data(deserial);
    return tx;
}

transaction_result::transaction_result(const slab_type slab, uint64_t size_limit)
  : slab_(slab), size_limit_(size_limit)
{
}

transaction_result::operator bool() const
{
    return slab_ != nullptr;
}

size_t transaction_result::height() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return from_little_endian_unsafe<uint32_t>(slab_);
}

size_t transaction_result::index() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return from_little_endian_unsafe<uint32_t>(slab_ + 4);
}

chain::transaction transaction_result::transaction() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
//    return deserialize_tx(slab_ + 8, size_limit_ - 8);
    return deserialize_tx(slab_ + 8);
}

transaction_database::transaction_database(
    const boost::filesystem::path& map_filename)
  : map_file_(map_filename), 
    header_(map_file_, 0),
    allocator_(map_file_, alloc_offset), 
    map_(header_, allocator_)
{
    BITCOIN_ASSERT(map_file_.data() != nullptr);
}

void transaction_database::create()
{
    map_file_.resize(initial_map_file_size);
    header_.create(number_buckets);
    allocator_.create();
}

void transaction_database::start()
{
    header_.start();
    allocator_.start();
}

transaction_result transaction_database::get(const hash_digest& hash) const
{
    const slab_type slab = map_.get(hash);
    return transaction_result(slab, allocator_.to_eof(slab));
}

void transaction_database::store(
    const transaction_metainfo& info, const chain::transaction& tx)
{
    // Write block data.
    const hash_digest key = tx.hash();
    const size_t value_size = 4 + 4 + tx.satoshi_size();
    auto write = [&info, &tx](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_4_bytes_little_endian(info.height);
        serial.write_4_bytes_little_endian(info.index);
        data_chunk tx_data = tx.to_data();
        serial.write_data(tx_data);
    };
    map_.store(key, write, value_size);
}

void transaction_database::remove(const hash_digest& hash)
{
    DEBUG_ONLY(bool success =) map_.unlink(hash);
    BITCOIN_ASSERT(success);
}

void transaction_database::sync()
{
    allocator_.sync();
}

} // namespace blockchain
} // namespace libbitcoin
