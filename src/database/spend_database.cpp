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

#include <bitcoin/blockchain/database/fsizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 228110589;
constexpr size_t header_size = htdb_record_header_fsize(number_buckets);
constexpr size_t initial_map_file_size = header_size + min_records_fsize;

constexpr position_type alloc_offset = header_size;
constexpr size_t value_size = hash_size + 4;
constexpr size_t record_size = record_fsize_htdb<hash_digest>(value_size);

// Create a new hash from a hash + index (a point)
// deterministically suitable for use in a hashtable.
// This technique could be replaced by simply using the output.hash.
static hash_digest output_to_hash(const output_point& output)
{
    data_chunk point(sizeof(output.index) + sizeof(output.hash));
    const auto index = to_little_endian(output.index);
    std::copy(output.hash.begin(), output.hash.end(), point.begin());
    std::copy(index.begin(), index.end(), point.begin() + sizeof(output.hash));

    // The index has a *very* low level of bit distribution evenness, 
    // almost none, and we must preserve the presumed random bit distribution,
    // so we need to re-hash here.
    return sha256_hash(point);
}

spend_result::spend_result(const record_type record)
  : record_(record)
{
}

spend_result::operator bool() const
{
    return record_ != nullptr;
}

hash_digest spend_result::hash() const
{
    BITCOIN_ASSERT(record_);
    hash_digest result;
    std::copy(record_, record_ + hash_size, result.begin());
    return result;
}

uint32_t spend_result::index() const
{
    BITCOIN_ASSERT(record_);
    return from_little_endian_unsafe<uint32_t>(record_ + hash_size);
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

spend_result spend_database::get(const output_point& outpoint) const
{
    const hash_digest key = output_to_hash(outpoint);
    const record_type record = map_.get(key);
    return spend_result(record);
}

void spend_database::store(
    const output_point& outpoint, const input_point& spend)
{
    const hash_digest key = output_to_hash(outpoint);
    auto write = [&spend](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_data(spend.hash);
        serial.write_4_bytes(spend.index);
    };
    map_.store(key, write);
}

void spend_database::remove(const output_point& outpoint)
{
    const hash_digest key = output_to_hash(outpoint);
    DEBUG_ONLY(bool success =) map_.unlink(key);
    BITCOIN_ASSERT(success);
}

void spend_database::sync()
{
    allocator_.sync();
}

spend_statinfo spend_database::statinfo() const
{
    return {
        header_.size(),
        allocator_.size()
    };
}

    } // namespace chain
} // namespace libbitcoin

