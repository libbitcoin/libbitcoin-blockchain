/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/test/unit_test.hpp>

#include <string>
#include <boost/filesystem.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;
using namespace bc::database;
using namespace bc::system;
using namespace boost::system;
using namespace boost::filesystem;

namespace test {

chain::block read_block(const std::string& hex)
{
    data_chunk data;
    BOOST_REQUIRE(decode_base16(data, hex));
    chain::block result;
    BOOST_REQUIRE(result.from_data(data));
    return result;
}

bool create_database(database::settings& out_database)
{
    static const auto mainnet = config::settings::mainnet;

    // Table optimization parameters, reduced for speed and more collision.
    out_database.file_growth_rate = 42;
    out_database.block_table_buckets = 42;
    out_database.transaction_table_buckets = 42;

    error_code ec;
    remove_all(out_database.directory, ec);
    database::data_base database(out_database, false, false);
    return create_directories(out_database.directory, ec) &&
        database.create(system::settings(mainnet).genesis_block);
}

} // namespace test
