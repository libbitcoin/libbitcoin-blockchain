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
#include <bitcoin/blockchain/database/hsdb_settings.hpp>

#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t settings_file_size = 8 * 6;

size_t hsdb_settings::number_shards() const
{
    return 1 << sharded_bitsize;
}

size_t hsdb_settings::scan_bitsize() const
{
    BITCOIN_ASSERT(total_key_size * 8 >= sharded_bitsize);
    return total_key_size * 8 - sharded_bitsize;
}
size_t hsdb_settings::scan_size() const
{
    const size_t bitsize = scan_bitsize();
    BITCOIN_ASSERT(bitsize != 0);
    const size_t size = (bitsize - 1) / 8 + 1;
    return size;
}
size_t hsdb_settings::number_buckets() const
{
    return 1 << bucket_bitsize;
}

hsdb_settings load_shard_settings(const mmfile& file)
{
    hsdb_settings settings;
    BITCOIN_ASSERT(file.size() == settings_file_size);
    auto deserial = make_deserializer(
        file.data(), file.data() + file.size());
    settings.version = deserial.read_4_bytes();
    settings.shard_max_entries = deserial.read_4_bytes();
    settings.total_key_size = deserial.read_4_bytes();
    settings.sharded_bitsize = deserial.read_4_bytes();
    settings.bucket_bitsize = deserial.read_4_bytes();
    settings.row_value_size = deserial.read_4_bytes();
    return settings;
}

void save_shard_settings(mmfile& file, const hsdb_settings& settings)
{
    bool success = file.resize(settings_file_size);
    BITCOIN_ASSERT(success);
    auto serial = make_serializer(file.data());
    serial.write_4_bytes(settings.version);
    serial.write_4_bytes(settings.shard_max_entries);
    serial.write_4_bytes(settings.total_key_size);
    serial.write_4_bytes(settings.sharded_bitsize);
    serial.write_4_bytes(settings.bucket_bitsize);
    serial.write_4_bytes(settings.row_value_size);
}

    } // namespace chain
} // namespace libbitcoin

