/*
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-blockchain.
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
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(block__tests)

BOOST_AUTO_TEST_CASE(block__genesis__mainnet__valid_structure)
{
    const auto genesis = mainnet_genesis_block();
    BOOST_REQUIRE(genesis.is_valid());
    BOOST_REQUIRE_EQUAL(genesis.transactions.size(), 1u);
    const auto root = block::generate_merkle_root(genesis.transactions);
    BOOST_REQUIRE(root == genesis.header.merkle);
}

BOOST_AUTO_TEST_CASE(block__genesis__testnet__valid_structure)
{
    const auto genesis = testnet_genesis_block();
    BOOST_REQUIRE(genesis.is_valid());
    BOOST_REQUIRE_EQUAL(genesis.transactions.size(), 1u);
    const auto root = block::generate_merkle_root(genesis.transactions);
    BOOST_REQUIRE(root == genesis.header.merkle);
}

BOOST_AUTO_TEST_SUITE_END()
