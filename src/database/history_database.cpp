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
#include <bitcoin/blockchain/database/history_database.hpp>

#include <bitcoin/blockchain/database/fsizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 10000;
constexpr size_t header_size = htdb_record_header_fsize(number_buckets);
constexpr size_t initial_lookup_file_size = header_size + min_records_fsize;

constexpr position_type alloc_offset = header_size;
constexpr size_t alloc_record_size = map_record_fsize_multimap<short_hash>();

constexpr size_t value_size = 36 + 4 + 8 + 36 + 4;
constexpr size_t row_record_size = record_fsize_htdb<hash_digest>(value_size);

history_database::history_database(
    const std::string& lookup_filename, const std::string& rows_filename)
  : lookup_file_(lookup_filename), header_(lookup_file_, 0),
    allocator_(lookup_file_, alloc_offset, alloc_record_size),
    start_lookup_(header_, allocator_),
    rows_file_(rows_filename), rows_(rows_file_, 0, row_record_size),
    linked_rows_(rows_),
    map_(start_lookup_, linked_rows_)
{
    BITCOIN_ASSERT(lookup_file_.data());
    BITCOIN_ASSERT(rows_file_.data());
}

void history_database::initialize_new()
{
    lookup_file_.resize(initial_lookup_file_size);
    header_.initialize_new(number_buckets);
    allocator_.initialize_new();

    rows_file_.resize(min_records_fsize);
    rows_.initialize_new();
}

void history_database::start()
{
    header_.start();
    allocator_.start();
    rows_.start();
}

constexpr size_t outpoint_size = hash_size + 4;
constexpr position_type spend_offset = outpoint_size + 4 + 8;
constexpr position_type spend_height_offset = spend_offset + outpoint_size;

void history_database::add_row(
    const short_hash& key, const output_point& outpoint,
    const uint32_t output_height, const uint64_t value)
{
    auto write = [&](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_hash(outpoint.hash);
        serial.write_4_bytes(outpoint.index);
        serial.write_4_bytes(output_height);
        serial.write_8_bytes(value);
        // Skip writing spend since it doesn't exist yet.
        serial.set_iterator(serial.iterator() + outpoint_size);
        serial.write_4_bytes(0);
    };
    map_.add_row(key, write);
}

void history_database::add_spend(
    const short_hash& key, const output_point& previous,
    const input_point& spend, const size_t spend_height)
{
    const index_type start = map_.lookup(key);
    for (const index_type index: multimap_iterable(linked_rows_, start))
    {
        const record_type data = linked_rows_.get(index);
        auto deserial = make_deserializer_unsafe(data);
        const output_point outpoint{
            deserial.read_hash(),
            deserial.read_4_bytes()};
        if (outpoint != previous)
            continue;
        auto serial = make_serializer(data + spend_offset);
        serial.write_hash(spend.hash);
        serial.write_4_bytes(spend.index);
        serial.write_4_bytes(spend_height);
        return;
    }
    BITCOIN_ASSERT(false);
}

void history_database::delete_spend(
    const short_hash& key, const input_point& spend)
{
    const index_type start = map_.lookup(key);
    for (const index_type index: multimap_iterable(linked_rows_, start))
    {
        const record_type data = linked_rows_.get(index);
        auto deserial = make_deserializer_unsafe(data + spend_offset);
        const input_point inpoint{
            deserial.read_hash(),
            deserial.read_4_bytes()};
        if (inpoint != spend)
            continue;
        auto serial = make_serializer(data + spend_height_offset);
        serial.write_4_bytes(0);
        return;
    }
    BITCOIN_ASSERT(false);
}

void history_database::delete_last_row(const short_hash& key)
{
    map_.delete_last_row(key);
}

history_result history_database::get(const short_hash& key,
    const size_t limit, index_type start)
{
    history_list history;
    auto read_row = [&history](const uint8_t* data)
    {
        auto deserial = make_deserializer_unsafe(data);
        return history_row{
            deserial.read_hash(),
            deserial.read_4_bytes(),
            deserial.read_4_bytes(),
            deserial.read_8_bytes(),
            deserial.read_hash(),
            deserial.read_4_bytes(),
            deserial.read_4_bytes()};
    };
    index_type stop = 0;
    if (!start)
        start = map_.lookup(key);
    for (const index_type index: multimap_iterable(linked_rows_, start))
    {
        if (limit && history.size() >= limit)
        {
            stop = index;
            break;
        }
        const record_type data = linked_rows_.get(index);
        history.emplace_back(read_row(data));
    }
    return history_result{std::move(history), stop};
}

void history_database::sync()
{
    allocator_.sync();
    rows_.sync();
}

    } // namespace chain
} // namespace libbitcoin

