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
#include <iostream>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;

// Create a new blockchain database.
int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "initchain: No directory specified." << std::endl;
        return 1;
    }
    const std::string prefix = argv[1];
    initialize_blockchain(prefix);
    // Add genesis block.
    db_paths paths(prefix);
    db_interface interface(paths, {0});
    interface.start();
    const block_type genesis = genesis_block();
    interface.push(genesis);
    return 0;
}

