/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
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
#include <iostream>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/database.hpp>

#define BS_INITCHAIN_DIR_NEW \
    "Failed to create directory %1% with error, '%2%'.\n"
#define BS_INITCHAIN_DIR_EXISTS \
    "Failed because the directory %1% already exists.\n"
#define BS_INITCHAIN_FAIL \
    "Failed to initialize blockchain files.\n"

using namespace bc;
using namespace bc::blockchain;
using namespace bc::database;
using namespace boost::filesystem;
using namespace boost::system;
using boost::format;

// Create a new mainnet blockchain database.
int main(int argc, char** argv)
{
    std::string prefix("mainnet");

    if (argc > 1)
        prefix = argv[1];

    if (argc > 2 && std::string("--clean") == argv[2])
        boost::filesystem::remove_all(prefix);

    error_code code;
    if (!create_directories(prefix, code))
    {
        if (code.value() == 0)
            std::cerr << format(BS_INITCHAIN_DIR_EXISTS) % prefix;
        else
            std::cerr << format(BS_INITCHAIN_DIR_NEW) % prefix %
                code.message();

        return -1;
    }

    if (!data_base::initialize(prefix, chain::block::genesis_mainnet()))
    {
        std::cerr << BS_INITCHAIN_FAIL;
        return -1;
    }

    return 0;
}
