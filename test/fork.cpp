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
using namespace bc::message;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(fork_tests)

#define DECLARE_BLOCK(block, number) \
    const auto block##number = std::make_shared<block_message>(); \
    block##number->header.bits = number;

// construct

BOOST_AUTO_TEST_CASE(fork__construct__default__zero)
{
    fork instance;
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), 0);
}

BOOST_AUTO_TEST_CASE(fork__construct__value__expected)
{
    static const size_t expected = 42;
    fork instance(expected);
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), expected);
}

// hash

BOOST_AUTO_TEST_CASE(fork__hash__default__null_hash)
{
    fork instance;
    BOOST_REQUIRE(instance.hash() == null_hash);
}

BOOST_AUTO_TEST_CASE(fork__hash__one_block__only_previous_block_hash)
{
    DECLARE_BLOCK(block, 1);
    DECLARE_BLOCK(block, 2);

    const auto expected = block1->hash();
    block2->header.previous_block_hash = expected;

    fork instance;
    BOOST_REQUIRE(instance.push(block2));
    BOOST_REQUIRE(instance.hash() == expected);
}

BOOST_AUTO_TEST_CASE(fork__hash__two_blocks__first_previous_block_hash)
{
    fork instance;
    DECLARE_BLOCK(top, 1);
    DECLARE_BLOCK(block, 2);
    DECLARE_BLOCK(block, 3);

    // Link the blocks.
    const auto expected = top1->hash();
    block2->header.previous_block_hash = expected;
    block3->header.previous_block_hash = block2->hash();

    BOOST_REQUIRE(instance.push(block2));
    BOOST_REQUIRE(instance.push(block3));
    BOOST_REQUIRE(instance.hash() == expected);
}

// difficulty

BOOST_AUTO_TEST_CASE(fork__difficulty__default__zero)
{
    fork instance;
    BOOST_REQUIRE_EQUAL(instance.difficulty().compact(), 0);
}

BOOST_AUTO_TEST_CASE(fork__difficulty__two_blocks__expected)
{
    fork instance;
    DECLARE_BLOCK(block, 1);
    DECLARE_BLOCK(block, 2);

    // Link the blocks.
    block2->header.previous_block_hash = block1->hash();

    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE(instance.push(block2));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    ///////////////////////////////////////////////////////////////////////////
    // TODO: devise value tests.
    ///////////////////////////////////////////////////////////////////////////
    BOOST_REQUIRE_EQUAL(instance.difficulty().compact(), 0);
}

// height/set_height

BOOST_AUTO_TEST_CASE(fork__height__default__zero)
{
    fork instance;
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

BOOST_AUTO_TEST_CASE(fork__set_height__round_trip__unchanged)
{
    static const size_t expected = 42;
    fork instance;
    instance.set_height(expected);
    BOOST_REQUIRE_EQUAL(instance.height(), expected);
}

// height_at

BOOST_AUTO_TEST_CASE(fork__height_at__zero__plus_one)
{
    static const size_t index = 0;
    static const size_t height = 42;
    static const size_t expected = height + index + 1;
    fork instance;
    instance.set_height(height);
    BOOST_REQUIRE_EQUAL(instance.height_at(index), expected);
}

BOOST_AUTO_TEST_CASE(fork__height_at__value__expected)
{
    static const size_t index = 10;
    static const size_t height = 42;
    static const size_t expected = height + index + 1;
    fork instance;
    instance.set_height(height);
    BOOST_REQUIRE_EQUAL(instance.height_at(index), expected);
}

// block_at

BOOST_AUTO_TEST_CASE(fork__block_at__default_zero__nullptr)
{
    fork instance;
    BOOST_REQUIRE(!instance.block_at(0));
}

BOOST_AUTO_TEST_CASE(fork__block_at__default_value__nullptr)
{
    fork instance;
    BOOST_REQUIRE(!instance.block_at(42));
}

// size

BOOST_AUTO_TEST_CASE(fork__size__empty__zero)
{
    fork instance;
    BOOST_REQUIRE_EQUAL(instance.size(), 0);
}

// empty

BOOST_AUTO_TEST_CASE(fork__empty__empty__true)
{
    fork instance;
    BOOST_REQUIRE(instance.empty());
}

BOOST_AUTO_TEST_CASE(fork__clear__default__empty_zero)
{
    fork instance;
    instance.clear();
    BOOST_REQUIRE(instance.empty());
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

// clear

BOOST_AUTO_TEST_CASE(fork__clear__set_height__zero)
{
    static const size_t height = 42;
    fork instance;
    instance.set_height(height);
    BOOST_REQUIRE_EQUAL(instance.height(), height);

    instance.clear();
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

BOOST_AUTO_TEST_CASE(fork__clear__capacity__zero)
{
    static const size_t capacity = 42;
    fork instance(capacity);
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), capacity);
    instance.clear();
    BOOST_REQUIRE_EQUAL(instance.blocks().capacity(), 0);
}

// blocks

BOOST_AUTO_TEST_CASE(fork__blocks__default__empty)
{
    fork instance;
    BOOST_REQUIRE(instance.blocks().empty());
}

// push

BOOST_AUTO_TEST_CASE(fork__push__one__success)
{
    fork instance;
    DECLARE_BLOCK(block, 1);
    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE(instance.block_at(0) == block1);
}

BOOST_AUTO_TEST_CASE(fork__push__two__success)
{
    fork instance;
    DECLARE_BLOCK(block, 1);
    DECLARE_BLOCK(block, 2);

    // Link the blocks.
    block2->header.previous_block_hash = block1->hash();

    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE(instance.push(block2));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE_EQUAL(instance.blocks().size(), 2u);
    BOOST_REQUIRE(instance.block_at(0) == block1);
    BOOST_REQUIRE(instance.block_at(1) == block2);
}

BOOST_AUTO_TEST_CASE(fork__push__unlinked__failure_on_second)
{
    fork instance;
    DECLARE_BLOCK(block, 1);
    DECLARE_BLOCK(block, 2);

    // Ensure the blocks are not linked.
    block2->header.previous_block_hash = null_hash;

    BOOST_REQUIRE(instance.push(block1));
    BOOST_REQUIRE(!instance.push(block2));
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE_EQUAL(instance.blocks().size(), 1u);
    BOOST_REQUIRE(instance.block_at(0) == block1);
}

BOOST_AUTO_TEST_SUITE_END()