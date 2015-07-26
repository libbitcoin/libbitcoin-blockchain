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
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace chain {

constexpr size_t number_buckets = 100000000;
BC_CONSTEXPR size_t header_size = htdb_slab_header_fsize(number_buckets);
BC_CONSTEXPR size_t initial_map_file_size = header_size + min_slab_fsize;

BC_CONSTEXPR position_type alloc_offset = header_size;

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
    BITCOIN_ASSERT(slab_ != nullptr);
    return from_little_endian_unsafe<uint32_t>(slab_);
}

size_t transaction_result::index() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return from_little_endian_unsafe<uint32_t>(slab_ + 4);
}

transaction_type transaction_result::transaction() const
{
    BITCOIN_ASSERT(slab_ != nullptr);
    return deserialize(slab_ + 8);
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
    return transaction_result(slab);
}

void transaction_database::store(
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

} // namespace chain
} // namespace libbitcoin

