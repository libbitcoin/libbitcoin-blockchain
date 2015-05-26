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

#include <boost/filesystem.hpp>

namespace libbitcoin {
namespace chain {

constexpr size_t bitfield_size = 4;

// ephemkey is without sign byte and address is without version byte.
// [ prefix_bitfield:4 ][ ephemkey:32 ][ address:20 ][ tx_id:32 ]
constexpr size_t row_size = 4 + 32 + 20 + 32;

stealth_database::stealth_database(const boost::filesystem::path& index_filename, 
    const boost::filesystem::path& rows_filename)
  : index_file_(index_filename), 
    index_(index_file_, 0, sizeof(index_type)),
    rows_file_(rows_filename), 
    rows_(rows_file_, 0, row_size)
{
}

void stealth_database::create()
{
    index_file_.resize(min_records_fsize);
    index_.create();
    rows_file_.resize(min_records_fsize);
    rows_.create();
}

void stealth_database::start()
{
    index_.start();
    rows_.start();
    block_start_ = rows_.count();
}

stealth_list stealth_database::scan(
    const binary_type& prefix, const size_t from_height) const
{
    if (from_height >= index_.count())
        return stealth_list();

    stealth_list result;
    const auto start = read_index(from_height);
    for (auto index = start; index < rows_.count(); ++index)
    {
        // see if prefix matches
        const auto record = rows_.get(index);
        const data_slice bitfield(record, record + bitfield_size);
        const binary_type compare(prefix.size(), bitfield);
        if (prefix != compare)
            continue;

        // Add row to results.
        auto deserial = make_deserializer_unsafe(record + bitfield_size);
        result.push_back(
        {
            deserial.read_hash(),
            deserial.read_short_hash(),
            deserial.read_hash()
        });
    }

    return result;
}

void stealth_database::store(
    const script_type& stealth_script, const stealth_row& row)
{
    // Create prefix.
    const auto prefix = calculate_stealth_prefix(stealth_script);
    BITCOIN_ASSERT(prefix.blocks().size() == bitfield_size);

    // Allocate new row.
    const auto index = rows_.allocate();
    const auto data = rows_.get(index);

    // Write data.
    auto serial = make_serializer(data);
    serial.write_data(prefix.blocks());
    serial.write_hash(row.ephemkey);
    serial.write_short_hash(row.address);
    serial.write_hash(row.transaction_hash);
    BITCOIN_ASSERT(serial.iterator() ==
        data + bitfield_size + hash_size + short_hash_size + hash_size);
}

void stealth_database::unlink(const size_t from_height)
{
    BITCOIN_ASSERT(index_.count() > from_height);
    index_.count(from_height);
}

void stealth_database::sync()
{
    rows_.sync();
    write_index();
}

void stealth_database::write_index()
{
    // Write index of first row into block lookup index.
    const index_type index = index_.allocate();
    record_type data = index_.get(index);
    auto serial = make_serializer(data);
    serial.write_4_bytes(block_start_);

    // Synchronise data.
    index_.sync();

    // Prepare for next block.
    block_start_ = rows_.count();
}

index_type stealth_database::read_index(const size_t from_height) const
{
    BITCOIN_ASSERT(from_height < index_.count());
    const auto record = index_.get(from_height);
    return from_little_endian_unsafe<index_type>(record);
}

} // namespace chain
} // namespace libbitcoin

