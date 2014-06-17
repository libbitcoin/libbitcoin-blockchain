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
#include <fstream>
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain/database/hsdb_settings.hpp>

using namespace libbitcoin;
using namespace libbitcoin::chain;

BOOST_AUTO_TEST_SUITE(hsdb_settings_tests)

void touch_file(const std::string& filename)
{
    std::ofstream outfile(filename);
    // Write byte so file is nonzero size.
    outfile.write("H", 1);
}

BOOST_AUTO_TEST_CASE(simple)
{
    touch_file("hsdb_settings");
    mmfile file("hsdb_settings");
    BOOST_REQUIRE(file.data());
    hsdb_shard_settings settings;
    settings.version = 110;
    settings.shard_max_entries = 1000000;
    settings.total_key_size = 20;
    settings.sharded_bitsize = 8;
    settings.bucket_bitsize = 8;
    settings.row_value_size = 49;
    // Save and reload.
    save_shard_settings(file, settings);
    hsdb_shard_settings settings1 = load_shard_settings(file);
    BOOST_REQUIRE(settings.version == settings1.version);
    BOOST_REQUIRE(settings.shard_max_entries == settings1.shard_max_entries);
    BOOST_REQUIRE(settings.total_key_size == settings1.total_key_size);
    BOOST_REQUIRE(settings.sharded_bitsize == settings1.sharded_bitsize);
    BOOST_REQUIRE(settings.bucket_bitsize == settings1.bucket_bitsize);
    BOOST_REQUIRE(settings.row_value_size == settings1.row_value_size);
}

BOOST_AUTO_TEST_SUITE_END()

