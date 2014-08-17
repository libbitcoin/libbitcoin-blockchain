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
#include <bitcoin/blockchain/database/spend_database.hpp>

#include <bitcoin/blockchain/database/sizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 10000;
constexpr size_t header_size = htdb_record_header_size(number_buckets);
constexpr size_t initial_map_file_size = header_size + min_records_size;

constexpr position_type alloc_offset = header_size;
constexpr size_t value_size = hash_size + 4;
constexpr size_t record_size = record_size_htdb<hash_digest>(value_size);

hash_digest tweak(hash_digest hash, const uint32_t index)
{
    for (uint8_t& byte: hash)
        byte = (byte + index) % std::numeric_limits<uint8_t>::max();
    return hash;
}

spend_database::spend_database(const std::string& filename)
  : file_(filename), header_(file_, 0),
    allocator_(file_, alloc_offset, record_size),
    map_(header_, allocator_)
{
    BITCOIN_ASSERT(file_.data());
}

void spend_database::initialize_new()
{
    file_.resize(initial_map_file_size);
    header_.initialize_new(number_buckets);
    allocator_.initialize_new();
}

void spend_database::start()
{
    header_.start();
    allocator_.start();
}

void spend_database::fetch(const output_point& outpoint,
    fetch_handler handle_fetch) const
{
    const hash_digest key = tweak(outpoint.hash, outpoint.index);
    const record_type record = map_.get(key);
    if (!record)
    {
        handle_fetch(error::not_found, input_point());
        return;
    }
    input_point spend;
    auto deserial = make_deserializer_unsafe(record);
    spend.hash = deserial.read_hash();
    spend.index = deserial.read_4_bytes();
    handle_fetch(std::error_code(), spend);
}

void spend_database::store(
    const output_point& outpoint, const input_point& spend)
{
    const hash_digest key = tweak(outpoint.hash, outpoint.index);
    auto write = [&spend](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_hash(spend.hash);
        serial.write_4_bytes(spend.index);
    };
    map_.store(key, write);
}

void spend_database::remove(const output_point& outpoint)
{
    const hash_digest key = tweak(outpoint.hash, outpoint.index);
    bool success = map_.unlink(key);
    BITCOIN_ASSERT(success);
}

void spend_database::sync()
{
    allocator_.sync();
}

    } // namespace chain
} // namespace libbitcoin

