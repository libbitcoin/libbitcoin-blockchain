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

#include <bitcoin/blockchain/database/sizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 10000;
constexpr size_t header_size = htdb_record_header_size(number_buckets);
constexpr size_t initial_map_file_size = header_size + min_records_size;

constexpr position_type alloc_offset = header_size;
constexpr size_t alloc_record_size = map_record_size_multimap<short_hash>();

constexpr size_t value_size = 36 + 4 + 8 + 36 + 4;
constexpr size_t row_record_size = record_size_htdb<hash_digest>(value_size);

history_database::history_database(
    const std::string& map_filename, const std::string& rows_filename)
  : map_file_(map_filename), header_(map_file_, 0),
    allocator_(map_file_, alloc_offset, alloc_record_size),
    start_lookup_(header_, allocator_),
    rows_file_(rows_filename), rows_(rows_file_, 0, row_record_size),
    linked_rows_(rows_),
    map_(start_lookup_, linked_rows_)
{
    BITCOIN_ASSERT(map_file_.data());
    BITCOIN_ASSERT(rows_file_.data());
}

void history_database::initialize_new()
{
    map_file_.resize(initial_map_file_size);
    header_.initialize_new(number_buckets);
    allocator_.initialize_new();

    rows_file_.resize(min_records_size);
    rows_.initialize_new();
}

void history_database::start()
{
    header_.start();
    allocator_.start();
    rows_.start();
}

void history_database::add_row(const short_hash& key, const history_row& row)
{
    auto write = [&row](uint8_t* data)
    {
        auto serial = make_serializer(data);
        serial.write_hash(row.output.hash);
        serial.write_4_bytes(row.output.index);
        serial.write_4_bytes(row.output_height);
        serial.write_8_bytes(row.value);
        serial.write_hash(row.spend.hash);
        serial.write_4_bytes(row.spend.index);
        serial.write_4_bytes(row.spend_height);
    };
    map_.add_row(key, write);
}

void history_database::fetch(
    const short_hash& key, fetch_handler handle_fetch,
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
    handle_fetch(std::error_code(), history, stop);
}

void history_database::delete_last_row(const short_hash& key)
{
    map_.delete_last_row(key);
}

    } // namespace chain
} // namespace libbitcoin

