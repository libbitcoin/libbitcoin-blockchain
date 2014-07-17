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
#include <bitcoin/blockchain/database/hsdb_shard.hpp>

#include <bitcoin/stealth.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/utility/logger.hpp>

namespace libbitcoin {
    namespace chain {

hsdb_shard::hsdb_shard(mmfile& file, const hsdb_settings& settings)
  : file_(file), settings_(settings)
{
}

void hsdb_shard::initialize_new()
{
    // Write header information.
    const size_t total_size = 8 + 8 * settings_.shard_max_entries;
    bool success = file_.resize(total_size);
    BITCOIN_ASSERT(success);
    auto serial = make_serializer(file_.data());
    const position_type initial_entries_end =
        8 + 8 * settings_.shard_max_entries;
    // No entries for now, so end position is data start.
    serial.write_8_bytes(initial_entries_end);
    // Zero out all block index entries.
    for (size_t i = 0; i < settings_.shard_max_entries; ++i)
        serial.write_8_bytes(0);
}

void hsdb_shard::start()
{
    // Read entries end.
    auto deserial = make_deserializer(file_.data(), file_.data() + 8);
    entries_end_ = deserial.read_8_bytes();
    // Check it's sane.
    const position_type initial_entries_end =
        8 + 8 * settings_.shard_max_entries;
    BITCOIN_ASSERT(entries_end_ >= initial_entries_end);
}

void hsdb_shard::add(const address_bitset& scan_key, const data_chunk& value)
{
    // Add rows to memory, synched to disk in later final step.
    BITCOIN_ASSERT(value.size() == settings_.row_value_size);
    BITCOIN_ASSERT(scan_key.size() == settings_.scan_bitsize());
    rows_.push_back(entry_row{scan_key, value});
}

void hsdb_shard::sort_rows()
{
    // Bitwise comparison.
    auto reverse_less_than = [](
        const address_bitset& bits_a, const address_bitset& bits_b)
    {
        BITCOIN_ASSERT(bits_a.size() == bits_b.size());
        for (size_t i = 0; i < bits_a.size(); ++i)
        {
            if (bits_a[i] != bits_b[i])
                return bits_a[i] < bits_b[i];
        }
        return false;
    };
    auto sort_func = [reverse_less_than](
        const entry_row& entry_a, const entry_row& entry_b)
    {
        return reverse_less_than(entry_a.scan_key, entry_b.scan_key);
    };
    // Sort in-memory rows so they are synched to disk in sorted order.
    std::sort(rows_.begin(), rows_.end(), sort_func);
}
void hsdb_shard::reserve(size_t space_needed)
{
    const size_t required_size = entries_end_ + space_needed;
    if (required_size <= file_.size())
        return;
    // Grow file by 1.5x
    const size_t new_size = required_size * 3 / 2;
    // Only ever grow file. Never shrink it!
    BITCOIN_ASSERT(new_size > file_.size());
    bool success = file_.resize(new_size);
    BITCOIN_ASSERT(success);
}
void hsdb_shard::link(const size_t height, const position_type entry)
{
    // Link latest block into header.
    position_type positions_bucket = 8 + 8 * height;
    auto serial_bucket = make_serializer(file_.data() + positions_bucket);
    serial_bucket.write_8_bytes(entry);
    // Extend entries end.
    auto serial_last = make_serializer(file_.data());
    serial_last.write_8_bytes(entries_end_);
}

index_type to_ulong_reverse(const address_bitset& key)
{
    index_type bucket = 0;
    for (size_t i = 0; i < key.size(); ++i)
    {
        const bool value = key[key.size() - i - 1];
        if (value)
            bucket += (1 << i);
    }
    return bucket;
}
index_type which_bucket(address_bitset key, const size_t bucket_bitsize)
{
    BITCOIN_ASSERT(bucket_bitsize <= sizeof(index_type) * 8);
    key.resize(bucket_bitsize);
    const index_type bucket = to_ulong_reverse(key);
    //std::cout << key << " = " << bucket << std::endl;
    return bucket;
}

template <typename Serializer>
void write_2_bytes_range(Serializer& serial,
    uint16_t value, size_t begin, size_t end)
{
    for (size_t i = begin; i < end; ++i)
        serial.write_2_bytes(value);
}
template <typename Serializer, typename Rows, typename Settings>
void write_block_buckets(Serializer& serial, Rows& rows, Settings& settings)
{
    const size_t number_buckets = settings.number_buckets();
    index_type begin_bucket = 0;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& row = rows[i];
        // Calculate bucket category for this row.
        const index_type end_bucket =
            which_bucket(row.scan_key, settings.bucket_bitsize) + 1;
        // Sometimes 2 keys can have the same prefix
        BITCOIN_ASSERT(begin_bucket <= end_bucket);
        const index_type value = i;
        // Write the indexes
        write_2_bytes_range(serial, value, begin_bucket, end_bucket);
        begin_bucket = end_bucket;
    }
    const index_type end_bucket = number_buckets;
    BITCOIN_ASSERT(begin_bucket < end_bucket);
    write_2_bytes_range(serial, rows.size(), begin_bucket, end_bucket);
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

void hsdb_shard::sync(size_t height)
{
#ifdef HSDB_DEBUG
    log_debug(LOG_HSDB) << "sync(" << height << ") [rows.size = "
        << rows_.size() << "]";
#endif
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
    write_block_buckets(serial, rows_, settings_);
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

position_type hsdb_shard::entry_position(size_t height) const
{
    // Find bucket in header.
    const index_type entry_bucket = height;
    const position_type bucket = 8 + 8 * entry_bucket;
    const uint8_t* begin = file_.data() + bucket;
    auto deserial = make_deserializer(begin, begin + 8);
    // Read entry position.
    const position_type entry = deserial.read_8_bytes();
    return entry;
}
size_t hsdb_shard::calc_entry_size(const position_type entry) const
{
    const uint8_t* begin = file_.data() + entry;
    auto deserial = make_deserializer(begin, begin + 2);
    // Read number of rows in entry.
    const size_t number_rows = deserial.read_2_bytes();
    const size_t row_size = settings_.scan_size() + settings_.row_value_size;
    const size_t entry_size =
        2 + 2 * settings_.number_buckets() + row_size * number_rows;
    return entry_size;
}
void hsdb_shard::unlink(size_t height)
{
    BITCOIN_ASSERT(height > 0);
    // Lookup entry_index at (height - 1). 
    const position_type prev_entry = entry_position(height - 1);
    // Calculate next_entry_position in entry.
    entries_end_ = prev_entry + calc_entry_size(prev_entry);
    // Set entries_end to next_entry_position.
    auto serial = make_serializer(file_.data());
    serial.write_8_bytes(entries_end_);
}

index_type read_row_index(
    const index_type bucket_index, const uint8_t* entry_begin)
{
    // Find bucket inside entry.
    const position_type bucket = 2 + 2 * bucket_index;
    auto deserial = make_deserializer(
        entry_begin + bucket, entry_begin + bucket + 2);
    // Read row index.
    const index_type row_index = deserial.read_2_bytes();
    return row_index;
}
void hsdb_shard::scan(const address_bitset& key,
    read_function read, size_t from_height) const
{
#ifdef HSDB_DEBUG
    log_debug(LOG_HSDB) << "scan(" << key << ", from_height="
        << from_height << ")";
#endif
    BITCOIN_ASSERT(key.size() <= settings_.scan_bitsize());
    // Jump to relevant entry, loop through entries until the end:
    position_type entry = entry_position(from_height);
    const index_type bucket_index =
        which_bucket(key, settings_.bucket_bitsize);
    while (entry != entries_end_)
    {
        const size_t entry_size = calc_entry_size(entry);
        const uint8_t* entry_begin = file_.data() + entry;
        const uint8_t* entry_end = entry_begin + entry_size;
        // Lookup start index from bucket. 
        const index_type row_index = read_row_index(bucket_index, entry_begin);
        // Scan row prefixes.
        const uint8_t* rows_sector =
            entry_begin + 2 + 2 * settings_.number_buckets();
        const size_t row_size =
            settings_.scan_size() + settings_.row_value_size;
        const uint8_t* current_row = rows_sector + row_size * row_index;
        while (current_row != entry_end)
        {
            // If not match, break
            if (!stealth_match(key, current_row))
                break;
            // Call read()
            const uint8_t* row_value = current_row + settings_.scan_size();
            read(row_value);
            // Keep scanning rows while they match.
            current_row += row_size;
        }
        // Next...
        entry += entry_size;
    }
}

    } // namespace chain
} // namespace libbitcoin

