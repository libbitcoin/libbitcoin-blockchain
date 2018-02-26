/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/blockchain.hpp>
#include "utility.hpp"

using namespace bc;
using namespace bc::blockchain;
using namespace bc::database;

#define TEST_SET_NAME \
    "fast_chain_tests"

class fast_chain_setup_fixture
{
public:
    fast_chain_setup_fixture()
    {
        log::initialize();
    }
};

BOOST_FIXTURE_TEST_SUITE(fast_chain_tests, fast_chain_setup_fixture)

BOOST_AUTO_TEST_CASE(block_chain__push__flushed__expected)
{
    START_BLOCKCHAIN(instance, true);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    const auto state1 = instance.get_block_state(block1->hash());
    BOOST_REQUIRE(is_confirmed(state1));
    const auto state0 = instance.get_block_state(chain::block::genesis_mainnet().hash());
    BOOST_REQUIRE(is_confirmed(state0));
}

BOOST_AUTO_TEST_CASE(block_chain__push__unflushed__expected_block)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    const auto state1 = instance.get_block_state(block1->hash());
    BOOST_REQUIRE(is_confirmed(state1));
    const auto state0 = instance.get_block_state(chain::block::genesis_mainnet().hash());
    BOOST_REQUIRE(is_confirmed(state0));
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_hash__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    hash_digest hash;
    BOOST_REQUIRE(!instance.get_block_hash(hash, 1, true));
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_hash__found__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));

    hash_digest hash;
    BOOST_REQUIRE(instance.get_block_hash(hash, 1, true));
    BOOST_REQUIRE(hash == block1->hash());
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__height_above_top__true)
{
    START_BLOCKCHAIN(instance, false);

    uint256_t work;
    uint256_t maximum(max_uint64);

    // This is allowed and just returns zero (standard new single block).
    BOOST_REQUIRE(instance.get_work(work, maximum, 1, true));
    BOOST_REQUIRE_EQUAL(work, 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__maximum_zero__true)
{
    START_BLOCKCHAIN(instance, false);

    uint256_t work;
    uint256_t maximum(0);

    // This should exit early due to hitting the maximum before the genesis block.
    BOOST_REQUIRE(instance.get_work(work, maximum, 0, true));
    BOOST_REQUIRE_EQUAL(work, 0);
}


BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__maximum_one__true)
{
    static const uint64_t genesis_mainnet_work = 0x0000000100010001;
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    uint256_t work;
    uint256_t maximum(genesis_mainnet_work);

    // This should exit early due to hitting the maximum on the genesis block.
    BOOST_REQUIRE(instance.get_work(work, maximum, 0, true));
    BOOST_REQUIRE_EQUAL(work, genesis_mainnet_work);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__unbounded__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));

    uint256_t work;
    uint256_t maximum(max_uint64);

    // This should not exit early.
    BOOST_REQUIRE(instance.get_work(work, maximum, 0, true));
    BOOST_REQUIRE_EQUAL(work, 0x0000000200020002);
}

BOOST_AUTO_TEST_CASE(block_chain__get_height__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    size_t height;
    BOOST_REQUIRE(!instance.get_block_height(height, null_hash, true));
}

BOOST_AUTO_TEST_CASE(block_chain__get_height__found__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));

    size_t height;
    BOOST_REQUIRE(instance.get_block_height(height, block1->hash(), true));
    BOOST_REQUIRE_EQUAL(height, 1u);
}

BOOST_AUTO_TEST_CASE(block_chain__get_bits__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    uint32_t bits;
    BOOST_REQUIRE(!instance.get_bits(bits, 1, true));
}

BOOST_AUTO_TEST_CASE(block_chain__get_bits__found__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));

    uint32_t bits;
    BOOST_REQUIRE(instance.get_bits(bits, 1, true));
    BOOST_REQUIRE_EQUAL(bits, block1->header().bits());
}

BOOST_AUTO_TEST_CASE(block_chain__get_timestamp__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    uint32_t timestamp;
    BOOST_REQUIRE(!instance.get_timestamp(timestamp, 1, true));
}

BOOST_AUTO_TEST_CASE(block_chain__get_timestamp__found__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));

    uint32_t timestamp;
    BOOST_REQUIRE(instance.get_timestamp(timestamp, 1, true));
    BOOST_REQUIRE_EQUAL(timestamp, block1->header().timestamp());
}

BOOST_AUTO_TEST_CASE(block_chain__get_version__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    uint32_t version;
    BOOST_REQUIRE(!instance.get_version(version, 1, true));
}

BOOST_AUTO_TEST_CASE(block_chain__get_version__found__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));

    uint32_t version;
    BOOST_REQUIRE(instance.get_version(version, 1, true));
    BOOST_REQUIRE_EQUAL(version, block1->header().version());
}

BOOST_AUTO_TEST_CASE(block_chain__get_top__no_gaps__last_block)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));

    config::checkpoint top;
    BOOST_REQUIRE(instance.get_top(top, true));
    BOOST_REQUIRE_EQUAL(top.height(), 2u);
}

BOOST_AUTO_TEST_CASE(block_chain__populate_output__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    const chain::output_point outpoint{ null_hash, 42 };
    size_t header_branch_height = 0;
    instance.populate_output(outpoint, header_branch_height);
    BOOST_REQUIRE(!outpoint.validation.cache.is_valid());
}

BOOST_AUTO_TEST_CASE(block_chain__populate_output__found__expected)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));

    const chain::output_point outpoint{ block2->transactions()[0].hash(), 0 };
    const auto expected_value = initial_block_subsidy_satoshi();
    const auto expected_script = block2->transactions()[0].outputs()[0].script().to_string(0);
    instance.populate_output(outpoint, 2);
    BOOST_REQUIRE(outpoint.validation.cache.is_valid());

    BOOST_REQUIRE(outpoint.validation.coinbase);
    BOOST_REQUIRE_EQUAL(outpoint.validation.height, 2u);
    BOOST_REQUIRE_EQUAL(outpoint.validation.cache.value(), expected_value);
    BOOST_REQUIRE_EQUAL(outpoint.validation.cache.script().to_string(0), expected_script);
}

BOOST_AUTO_TEST_CASE(block_chain__populate_output__below_fork__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));

    const chain::output_point outpoint{ block2->transactions().front().hash(), 0 };
    instance.populate_output(outpoint, 3);
    BOOST_REQUIRE(outpoint.validation.cache.is_valid());
}

BOOST_AUTO_TEST_CASE(block_chain__populate_output__at_fork__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));

    const chain::output_point outpoint{ block2->transactions().front().hash(), 0 };
    instance.populate_output(outpoint, 2);
    BOOST_REQUIRE(outpoint.validation.cache.is_valid());
}

BOOST_AUTO_TEST_CASE(block_chain__populate_output__above_fork__false)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));

    const chain::output_point outpoint{ block2->transactions().front().hash(), 0 };
    instance.populate_output(outpoint, 1);
    BOOST_REQUIRE(!outpoint.validation.cache.is_valid());
}

BOOST_AUTO_TEST_SUITE_END()

