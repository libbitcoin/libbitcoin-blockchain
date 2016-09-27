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
#include <boost/test/unit_test.hpp>

#include <memory>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::message;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(fork_tests)

#define DECLARE_BLOCK(block, number) \
    const auto block##number = std::make_shared<block_message>(); \
    block##number->header.bits = number;

// construct

BOOST_AUTO_TEST_CASE(fork__construct__default__zero)
{
    blockchain::fork instance;
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), 0);
}

BOOST_AUTO_TEST_CASE(fork__construct__value__expected)
{
    static const size_t expected = 42;
    blockchain::fork instance(expected);
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), expected);
}

// hash

BOOST_AUTO_TEST_CASE(fork__hash__default__null_hash)
{
    blockchain::fork instance;
    BOOST_REQUIRE(instance.hash() == null_hash);
}

BOOST_AUTO_TEST_CASE(fork__hash__one_block__only_previous_block_hash)
{
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    const auto expected = block0->hash();
    block1->header.previous_block_hash = expected;

    blockchain::fork instance;
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE(instance.hash() == expected);
}

BOOST_AUTO_TEST_CASE(fork__hash__two_blocks__first_previous_block_hash)
{
    blockchain::fork instance;
    DECLARE_BLOCK(top, 42);
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    const auto expected = top42->hash();
    block0->header.previous_block_hash = expected;
    block1->header.previous_block_hash = block0->hash();

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE(instance.hash() == expected);
}

// height/set_height

BOOST_AUTO_TEST_CASE(fork__height__default__zero)
{
    blockchain::fork instance;
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

BOOST_AUTO_TEST_CASE(fork__set_height__round_trip__unchanged)
{
    static const size_t expected = 42;
    blockchain::fork instance;
    instance.set_height(expected);
    BOOST_REQUIRE_EQUAL(instance.height(), expected);
}

// height_at

BOOST_AUTO_TEST_CASE(fork__height_at__zero__plus_one)
{
    static const size_t index = 0;
    static const size_t height = 42;
    static const size_t expected = height + index + 1;
    blockchain::fork instance;
    instance.set_height(height);
    BOOST_REQUIRE_EQUAL(instance.height_at(index), expected);
}

BOOST_AUTO_TEST_CASE(fork__height_at__value__expected)
{
    static const size_t index = 10;
    static const size_t height = 42;
    static const size_t expected = height + index + 1;
    blockchain::fork instance;
    instance.set_height(height);
    BOOST_REQUIRE_EQUAL(instance.height_at(index), expected);
}

// block_at

BOOST_AUTO_TEST_CASE(fork__block_at__default_zero__nullptr)
{
    blockchain::fork instance;
    BOOST_REQUIRE(!instance.block_at(0));
}

BOOST_AUTO_TEST_CASE(fork__block_at__default_value__nullptr)
{
    blockchain::fork instance;
    BOOST_REQUIRE(!instance.block_at(42));
}

// size

BOOST_AUTO_TEST_CASE(fork__size__empty__zero)
{
    blockchain::fork instance;
    BOOST_REQUIRE_EQUAL(instance.size(), 0);
}

// empty

BOOST_AUTO_TEST_CASE(fork__empty__empty__true)
{
    blockchain::fork instance;
    BOOST_REQUIRE(instance.empty());
}

BOOST_AUTO_TEST_CASE(fork__clear__default__empty_zero)
{
    blockchain::fork instance;
    instance.clear();
    BOOST_REQUIRE(instance.empty());
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

// clear

BOOST_AUTO_TEST_CASE(fork__clear__set_height__zero)
{
    static const size_t height = 42;
    blockchain::fork instance;
    instance.set_height(height);
    BOOST_REQUIRE_EQUAL(instance.height(), height);

    instance.clear();
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

BOOST_AUTO_TEST_CASE(fork__clear__capacity__zero)
{
    static const size_t capacity = 42;
    blockchain::fork instance(capacity);
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), capacity);
    instance.clear();
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), 0);
}

// blocks

BOOST_AUTO_TEST_CASE(fork__blocks__default__empty)
{
    blockchain::fork instance;
    BOOST_REQUIRE(instance.blocks().empty());
}

BOOST_AUTO_TEST_CASE(fork__blocks__one__empty)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.blocks().size(), 1u);
    instance.clear();
    BOOST_REQUIRE(instance.blocks().empty());
}

// push

BOOST_AUTO_TEST_CASE(fork__push__one__success)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE(instance.block_at(0) == block0);
}

BOOST_AUTO_TEST_CASE(fork__push__two_linked__success)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header.previous_block_hash = block0->hash();

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE(instance.block_at(0) == block0);
    BOOST_REQUIRE(instance.block_at(1) == block1);
}

BOOST_AUTO_TEST_CASE(fork__push__two_unlinked__failure_on_second)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Ensure the blocks are not linked.
    block1->header.previous_block_hash = null_hash;

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(!instance.push(block1));
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE(instance.block_at(0) == block0);
}

// pop

BOOST_AUTO_TEST_CASE(fork__pop__one_of_two__first_remains)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header.previous_block_hash = block0->hash();

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto list = instance.pop(1, error::invalid_proof_of_work);
    BOOST_REQUIRE_EQUAL(list.size(), 1u);
    BOOST_REQUIRE(instance.block_at(0) == block0);

    const auto first = list.front();
    BOOST_REQUIRE(first == block1);
    BOOST_REQUIRE_EQUAL(first->metadata.validation_result, error::invalid_proof_of_work);
    BOOST_REQUIRE_EQUAL(first->metadata.validation_height, block::metadata::orphan_height);
}

BOOST_AUTO_TEST_CASE(fork__pop__two_of_two__none_remain)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header.previous_block_hash = block0->hash();

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto list = instance.pop(0, error::invalid_proof_of_work);
    BOOST_REQUIRE_EQUAL(list.size(), 2u);
    BOOST_REQUIRE(instance.empty());
    BOOST_REQUIRE(list[0] == block0);
    BOOST_REQUIRE(list[1] == block1);
}

BOOST_AUTO_TEST_CASE(fork__pop__three_of_two__unchanged_fork_empty_return)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header.previous_block_hash = block0->hash();

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE(instance.pop(2, error::invalid_proof_of_work).empty());
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
}

// is_verified

BOOST_AUTO_TEST_CASE(fork__is_verified__default__false)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE(!instance.is_verified(0));
}

// set_verified

BOOST_AUTO_TEST_CASE(fork__set_verified__first__round_trips)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE(!instance.is_verified(0));
    instance.set_verified(0);
    BOOST_REQUIRE(instance.is_verified(0));
}

// difficulty

BOOST_AUTO_TEST_CASE(fork__difficulty__default__zero)
{
    blockchain::fork instance;
    BOOST_REQUIRE_EQUAL(instance.difficulty().compact(), 0);
}

BOOST_AUTO_TEST_CASE(fork__difficulty__two_blocks__expected)
{
    blockchain::fork instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header.previous_block_hash = block0->hash();

    BOOST_REQUIRE(instance.push(block0));
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    ///////////////////////////////////////////////////////////////////////////
    // TODO: devise value tests.
    ///////////////////////////////////////////////////////////////////////////
    BOOST_REQUIRE_EQUAL(instance.difficulty().compact(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
