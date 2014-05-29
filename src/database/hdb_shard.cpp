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
#include <bitcoin/blockchain/database/hdb_shard.hpp>

#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/blockchain/database/util.hpp>

namespace libbitcoin {
    namespace blockchain {

size_t hdb_shard_settings::scan_bitsize() const
{
    BITCOIN_ASSERT(total_key_size * 8 >= sharded_bitsize);
    return total_key_size * 8 - sharded_bitsize;
}
size_t hdb_shard_settings::scan_size() const
{
    const size_t bitsize = scan_bitsize();
    BITCOIN_ASSERT(bitsize != 0);
    const size_t size = (bitsize - 1) / 8 + 1;
    return size;
}
size_t hdb_shard_settings::number_buckets() const
{
    return 1 << bucket_bitsize;
}

hdb_shard::hdb_shard(mmfile& file, const hdb_shard_settings& settings)
  : file_(file), settings_(settings)
{
}

void hdb_shard::initialize_new()
{
    constexpr size_t total_size = 8 + 8 * shard_max_entries;
    bool success = file_.resize(total_size);
    BITCOIN_ASSERT(success);
    auto serial = make_serializer(file_.data());
    constexpr position_type initial_entries_end = 8 + 8 * shard_max_entries;
    serial.write_8_bytes(initial_entries_end);
    for (size_t i = 0; i < shard_max_entries; ++i)
        serial.write_8_bytes(0);
}

void hdb_shard::start()
{
    auto deserial = make_deserializer(file_.data(), file_.data() + 8);
    entries_end_ = deserial.read_8_bytes();
    constexpr position_type initial_entries_end = 8 + 8 * shard_max_entries;
    BITCOIN_ASSERT(entries_end_ >= initial_entries_end);
}

void hdb_shard::add(const address_bitset& scan_key, const data_chunk& value)
{
    BITCOIN_ASSERT(value.size() == settings_.row_value_size);
    const size_t scan_bits =
        settings_.total_key_size * 8 - settings_.sharded_bitsize;
    BITCOIN_ASSERT(scan_key.size() == scan_bits);
    rows_.push_back(entry_row{scan_key, value});
}

void hdb_shard::sort_rows()
{
    auto sort_func = [](const entry_row& entry_a, const entry_row& entry_b)
    {
        return entry_a.scan_key < entry_b.scan_key;
    };
    std::sort(rows_.begin(), rows_.end(), sort_func);
}
void hdb_shard::reserve(size_t space_needed)
{
    const size_t required_size = entries_end_ + space_needed;
    if (required_size <= file_.size())
        return;
    const size_t new_size = required_size * 3 / 2;
    // Only ever grow file. Never shrink it!
    BITCOIN_ASSERT(new_size > file_.size());
    bool success = file_.resize(new_size);
    BITCOIN_ASSERT(success);
}
void hdb_shard::link(const size_t height, const position_type entry)
{
    position_type positions_bucket = 8 + 8 * height;
    auto serial_bucket = make_serializer(file_.data() + positions_bucket);
    serial_bucket.write_8_bytes(entry);
    auto serial_last = make_serializer(file_.data());
    serial_last.write_8_bytes(entries_end_);
}

template <typename Serializer>
void write_2_bytes(Serializer& serial,
    uint16_t value, size_t begin, size_t end)
{
    for (size_t i = begin; i < end; ++i)
        serial.write_2_bytes(value);
}
template <typename Row>
size_t which_bucket(Row& row, size_t bucket_bitsize)
{
    address_bitset prefix = row.scan_key;
    prefix_resize(prefix, bucket_bitsize);
    const size_t bucket = prefix.to_ulong();
    std::cout << prefix << " = " << bucket << std::endl;
    return bucket;
}
template <typename Serializer, typename Rows, typename Settings>
void write_buckets(Serializer& serial, Rows& rows, Settings& settings)
{
    const size_t number_buckets = settings.number_buckets();
    size_t begin_bucket = 0;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& row = rows[i];
        // Calculate bucket category for this row.
        const size_t end_bucket =
            which_bucket(row, settings.bucket_bitsize) + 1;
        BITCOIN_ASSERT(begin_bucket < end_bucket);
        const index_type value = i;
        // Write the indexes
        write_2_bytes(serial, value, begin_bucket, end_bucket);
        begin_bucket = end_bucket;
    }
    const size_t end_bucket = number_buckets;
    BITCOIN_ASSERT(begin_bucket < end_bucket);
    write_2_bytes(serial, rows.size(), begin_bucket, end_bucket);
}

template <typename Serializer, typename Rows, typename Settings>
void write_rows(Serializer& serial, Rows& rows, Settings& settings)
{
    const size_t scan_size = settings.scan_size();
    for (const auto& row: rows)
    {
        // Convert key to data
        BITCOIN_ASSERT(scan_size == row.scan_key.num_blocks());
        data_chunk scan_data(scan_size);
        boost::to_block_range(row.scan_key, scan_data.begin());
        // Write key data
        serial.write_data(scan_data);
        BITCOIN_ASSERT(row.value.size() == settings.row_value_size);
        // Write value
        serial.write_data(row.value);
    }
}

void hdb_shard::sync(size_t height)
{
    sort_rows();
    // Calc space needed + reserve.
    const size_t row_size = settings_.scan_size() + settings_.row_value_size;
    const size_t entry_header_size = 2 + 2 * settings_.number_buckets();
    const size_t entry_size = entry_header_size + row_size * rows_.size();
    reserve(entry_size);
    const position_type entry_position = entries_end_;
    auto serial = make_serializer(file_.data() + entry_position);
    serial.write_2_bytes(rows_.size());
    // Write buckets.
    write_buckets(serial, rows_, settings_);
    const position_type rows_sector = entry_position + entry_header_size;
    BITCOIN_ASSERT(serial.iterator() == file_.data() + rows_sector);
    // Write rows.
    write_rows(serial, rows_, settings_);
    rows_.clear();
    // Relocate entries_end.
    entries_end_ += entry_size;
    BITCOIN_ASSERT(serial.iterator() == file_.data() + entries_end_);
    // Link
    link(height, entry_position);
}

    } // namespace blockchain
} // namespace libbitcoin

