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
#include <bitcoin/blockchain/database/history_scan_database.hpp>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

std::string settings_path(const std::string& prefix)
{
    using boost::filesystem::path;
    path file_path = path(prefix) / "settings";
    return file_path.generic_string();
}

std::string shard_path(const std::string& prefix,
    const size_t i, const hsdb_settings& settings)
{
    using boost::filesystem::path;
    address_bitset scan_prefix(settings.sharded_bitsize, i);
    std::string prefix_str;
    boost::to_string(scan_prefix, prefix_str);
    path file_path = path(prefix) / (std::string("shard_") + prefix_str);
    return file_path.generic_string();
}

void create_hsdb(const std::string& prefix,
    const hsdb_settings& settings)
{
    // Create settings file /path/settings
    touch_file(settings_path(prefix));
    mmfile settings_file(settings_path(prefix));
    save_hsdb_settings(settings_file, settings);
    // Create each shard
    for (size_t i = 0; i < settings.number_shards(); ++i)
    {
        // /path/shard_0101101...
        const std::string filename = shard_path(prefix, i, settings);
        touch_file(filename);
        mmfile file(filename);
        // Initialize new shard file.
        hsdb_shard shard(file, settings);
        shard.initialize_new();
    }
}

history_scan_database::history_scan_database(const std::string& prefix)
{
    // Load settings file /path/settings
    mmfile settings_file(settings_path(prefix));
    settings_ = load_hsdb_settings(settings_file);
    for (size_t i = 0; i < settings_.number_shards(); ++i)
    {
        // Open each shard file.
        const std::string filename = shard_path(prefix, i, settings_);
        files_.emplace_back(filename);
    }
    for (size_t i = 0; i < settings_.number_shards(); ++i)
    {
        BITCOIN_ASSERT(files_[i].data());
        // Start each shard.
        hsdb_shard shard(files_[i], settings_);
        shard.start();
        shards_.emplace_back(std::move(shard));
    }
}

void history_scan_database::add_output(
    const address_bitset& key, const output_point& outpoint,
    const uint32_t output_height, const uint64_t value)
{
    add(key, 0, outpoint, output_height, value);
}

void history_scan_database::add_spend(
    const address_bitset& key, const output_point& previous,
    const input_point& spend, const size_t spend_height)
{
    add(key, 1, spend, spend_height, spend_checksum(previous));
}

void history_scan_database::add(const address_bitset& key,
    const uint8_t marker, const point_type& point,
    uint32_t block_height, uint64_t value)
{
    BITCOIN_ASSERT(key.size() >= settings_.sharded_bitsize);
    // Both add() and sync() must have identical lookup of shards.
    hsdb_shard& shard = lookup(key);
    address_bitset sub_key = drop_prefix(key);
    BITCOIN_ASSERT(sub_key.size() == settings_.scan_bitsize());
#ifdef HSDB_DEBUG
    log_debug(LOG_HSDB) << "Sub key = " << sub_key;
#endif
    data_chunk row_data(settings_.row_value_size);
    auto serial = make_serializer(row_data.begin());
    serial.write_byte(marker);
    serial.write_hash(point.hash);
    serial.write_4_bytes(point.index);
    serial.write_4_bytes(block_height);
    serial.write_8_bytes(value);
    BITCOIN_ASSERT(serial.iterator() ==
        row_data.begin() + settings_.row_value_size);
    shard.add(sub_key, row_data);
}

void history_scan_database::sync(size_t height)
{
    for (hsdb_shard& shard: shards_)
        shard.sync(height);
}

void history_scan_database::unlink(size_t height)
{
    for (hsdb_shard& shard: shards_)
        shard.unlink(height);
}

// Each row contains a start byte which signifies whether
// it is an output or a spend.
inline point_ident marker_to_id(uint8_t marker)
{
    BITCOIN_ASSERT(marker == 0 || marker == 1);
    if (marker == 0)
        return point_ident::output;
    return point_ident::spend;
}

void history_scan_database::scan(const address_bitset& key,
    read_function read_func, size_t from_height) const
{
    BITCOIN_ASSERT(key.size() >= settings_.sharded_bitsize);
    const hsdb_shard& shard = lookup(key);
    address_bitset sub_key = drop_prefix(key);
    auto read_wrapped = [&read_func](const uint8_t* data)
    {
        auto deserial = make_deserializer_unsafe(data);
        history_row row{
            // output or spend?
            marker_to_id(deserial.read_byte()),
            // point
            deserial.read_hash(),
            deserial.read_4_bytes(),
            // height
            deserial.read_4_bytes(),
            // value or checksum
            deserial.read_8_bytes()};
        read_func(row);
    };
    shard.scan(sub_key, read_wrapped, from_height);
}

template <typename ShardRef, typename ShardList, typename Settings>
ShardRef lookup_impl(ShardList& shards, const Settings& settings,
    address_bitset key)
{
    key.resize(settings.sharded_bitsize);
    size_t idx = key.to_ulong();
#ifdef HSDB_DEBUG
    log_debug(LOG_HSDB) << "Using key " << key << " (" << idx << ")";
#endif
    return shards[idx];
}

hsdb_shard& history_scan_database::lookup(address_bitset key)
{
    return lookup_impl<hsdb_shard&>(shards_, settings_, key);
}
const hsdb_shard& history_scan_database::lookup(address_bitset key) const
{
    return lookup_impl<const hsdb_shard&>(shards_, settings_, key);
}

address_bitset history_scan_database::drop_prefix(address_bitset key) const
{
    // Drop first sharded_bitsize bits from key.
    const size_t drop_size = settings_.sharded_bitsize;
    BITCOIN_ASSERT(key.size() >= drop_size);
    key >>= drop_size;
    key.resize(key.size() - drop_size);
    BITCOIN_ASSERT(key.size() <= settings_.scan_bitsize());
    return key;
}

    } // namespace chain
} // namespace libbitcoin

