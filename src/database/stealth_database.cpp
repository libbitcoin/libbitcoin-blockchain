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
#include <bitcoin/blockchain/database/stealth_database.hpp>

#include <bitcoin/blockchain/database/fsizes.hpp>

namespace libbitcoin {
    namespace chain {

// ephemkey is without sign byte and address is without version byte.
// [ prefix_bitfield:4 ][ ephemkey:32 ][ address:20 ][ tx_id:32 ]
constexpr size_t row_size = 4 + 32 + 20 + 32;

stealth_database::stealth_database(
    const std::string& index_filename, const std::string& rows_filename)
  : index_file_(index_filename), index_(index_file_, 0, sizeof(index_type)),
    rows_file_(rows_filename), rows_(rows_file_, 0, row_size)
{
}

void stealth_database::initialize_new()
{
    index_file_.resize(min_records_fsize);
    index_.initialize_new();

    rows_file_.resize(min_records_fsize);
    rows_.initialize_new();
}

void stealth_database::start()
{
    index_.start();
    rows_.start();
    block_start_ = rows_.size();
}

stealth_list stealth_database::scan(
    const stealth_prefix& prefix, const size_t from_height) const
{
    if (from_height >= index_.size())
        return stealth_list();
    stealth_list result;
    index_type start = read_index(from_height);
    for (size_t index = start; index < rows_.size(); ++index)
    {
        // see if prefix matches
        const record_type record = rows_.get(index);
        if (!stealth_match(prefix, record))
            continue;
        // Add row to results.
        auto deserial = make_deserializer_unsafe(record + 4);
        result.push_back({
            deserial.read_hash(),
            deserial.read_short_hash(),
            deserial.read_hash()
        });
    }
    return result;
}

void stealth_database::store(
    const stealth_bitfield bitfield, const stealth_row& row)
{
    const index_type idx = rows_.allocate();
    record_type record = rows_.get(idx);
    auto serial = make_serializer(record);
    serial.write_4_bytes(bitfield);
    serial.write_hash(row.ephemkey);
    serial.write_short_hash(row.address);
    serial.write_hash(row.transaction_hash);
}

void stealth_database::unlink(const size_t from_height)
{
    BITCOIN_ASSERT(index_.size() > from_height);
    index_.resize(from_height);
}

void stealth_database::sync()
{
    rows_.sync();
    write_index();
}

void stealth_database::write_index()
{
    // Write index of first row into block lookup index.
    const index_type idx = index_.allocate();
    record_type record = index_.get(idx);
    auto serial = make_serializer(record);
    BITCOIN_ASSERT(sizeof(block_start_) == 4);
    serial.write_4_bytes(block_start_);
    // Synchronise data.
    index_.sync();
    // Prepare for next block.
    block_start_ = rows_.size();
}

index_type stealth_database::read_index(const size_t from_height) const
{
    BITCOIN_ASSERT(from_height < index_.size());
    const record_type record = index_.get(from_height);
    return from_little_endian<index_type>(record);
}

    } // namespace chain
} // namespace libbitcoin

