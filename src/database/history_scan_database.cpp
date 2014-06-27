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

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/logger.hpp>

namespace libbitcoin {
    namespace chain {

void touch_file(const std::string& filename)
{
    std::ofstream outfile(filename);
    // Write byte so file is nonzero size.
    outfile.write("H", 1);
}

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
    save_shard_settings(settings_file, settings);
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
    settings_ = load_shard_settings(settings_file);
    for (size_t i = 0; i < settings_.number_shards(); ++i)
    {
        // Open each shard file.
        const std::string filename = shard_path(prefix, i, settings_);
        mmfile file(filename);
        BITCOIN_ASSERT(file.data());
        shard_files_.emplace_back(std::move(file));
    }
    for (size_t i = 0; i < settings_.number_shards(); ++i)
    {
        BITCOIN_ASSERT(shard_files_[i].data());
        // Start each shard.
        hsdb_shard shard(shard_files_[i], settings_);
        shard.start();
        shards_.emplace_back(std::move(shard));
    }
}

void history_scan_database::add(
    const address_bitset& key, const data_chunk& value)
{
    BITCOIN_ASSERT(key.size() >= settings_.sharded_bitsize);
    // Both add() and sync() must have identical lookup of shards.
    hsdb_shard& shard = lookup(key);
    address_bitset sub_key = drop_prefix(key);
    std::cout << sub_key.size() << std::endl;
    BITCOIN_ASSERT(sub_key.size() == settings_.scan_bitsize());
#ifdef HSDB_DEBUG
    log_debug(LOG_HSDB) << "Sub key " << sub_key;
#endif
    shard.add(sub_key, value);
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

void history_scan_database::scan(const address_bitset& key,
    read_function read, size_t from_height)
{
    BITCOIN_ASSERT(key.size() >= settings_.sharded_bitsize);
    hsdb_shard& shard = lookup(key);
    address_bitset sub_key = drop_prefix(key);
    shard.scan(sub_key, read, from_height);
}

address_bitset history_scan_database::drop_prefix(address_bitset key)
{
    // Drop first sharded_bitsize bits from key.
    const size_t drop_size = settings_.sharded_bitsize;
    BITCOIN_ASSERT(key.size() >= drop_size);
    key >>= drop_size;
    key.resize(key.size() - drop_size);
    BITCOIN_ASSERT(key.size() <= settings_.scan_bitsize());
    return key;
}

hsdb_shard& history_scan_database::lookup(address_bitset key)
{
    key.resize(settings_.sharded_bitsize);
    size_t idx = key.to_ulong();
#ifdef HSDB_DEBUG
    log_debug(LOG_HSDB) << "Using key " << key << " (" << idx << ")";
#endif
    return shards_[idx];
}

    } // namespace chain
} // namespace libbitcoin

