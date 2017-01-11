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

BOOST_AUTO_TEST_SUITE(branch_tests)

#define DECLARE_BLOCK(name, number) \
    const auto name##number = std::make_shared<block>(); \
    name##number->header().set_bits(number);

// Access to protected members.
class branch_fixture
  : public branch
{
public:
    size_t index_of(size_t height) const
    {
        return branch::index_of(height);
    }

    size_t height_at(size_t index) const
    {
        return branch::height_at(index);
    }
};

// construct

BOOST_AUTO_TEST_CASE(branch__construct__always__capacity_1)
{
    branch instance;
    BOOST_REQUIRE_EQUAL(instance.blocks()->capacity(), 1u);
}

// hash

BOOST_AUTO_TEST_CASE(branch__hash__default__null_hash)
{
    branch instance;
    BOOST_REQUIRE(instance.hash() == null_hash);
}

BOOST_AUTO_TEST_CASE(branch__hash__one_block__only_previous_block_hash)
{
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    const auto expected = block0->hash();
    block1->header().set_previous_block_hash(expected);

    branch instance;
    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(instance.hash() == expected);
}

BOOST_AUTO_TEST_CASE(branch__hash__two_blocks__first_previous_block_hash)
{
    branch instance;
    DECLARE_BLOCK(top, 42);
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    const auto expected = top42->hash();
    block0->header().set_previous_block_hash(expected);
    block1->header().set_previous_block_hash(block0->hash());

    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE(instance.hash() == expected);
}

// height/set_height

BOOST_AUTO_TEST_CASE(branch__height__default__zero)
{
    branch instance;
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

BOOST_AUTO_TEST_CASE(branch__set_height__round_trip__unchanged)
{
    static const size_t expected = 42;
    branch instance;
    instance.set_height(expected);
    BOOST_REQUIRE_EQUAL(instance.height(), expected);
}

// index_of

BOOST_AUTO_TEST_CASE(branch__index_of__one__zero)
{
    branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.index_of(1), 0u);
}

BOOST_AUTO_TEST_CASE(branch__index_of__two__one)
{
    branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.index_of(2), 1u);
}

BOOST_AUTO_TEST_CASE(branch__index_of__value__expected)
{
    branch_fixture instance;
    instance.set_height(42);
    BOOST_REQUIRE_EQUAL(instance.index_of(53), 10u);
}

// height_at

BOOST_AUTO_TEST_CASE(branch__height_at__zero__one)
{
    branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.height_at(0), 1u);
}

BOOST_AUTO_TEST_CASE(branch__height_at__one__two)
{
    branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.height_at(1), 2u);
}

BOOST_AUTO_TEST_CASE(branch__height_at__value__expected)
{
    branch_fixture instance;
    instance.set_height(42);
    BOOST_REQUIRE_EQUAL(instance.height_at(10), 53u);
}

// size

BOOST_AUTO_TEST_CASE(branch__size__empty__zero)
{
    branch instance;
    BOOST_REQUIRE_EQUAL(instance.size(), 0);
}

// empty

BOOST_AUTO_TEST_CASE(branch__empty__default__true)
{
    branch instance;
    BOOST_REQUIRE(instance.empty());
}

BOOST_AUTO_TEST_CASE(branch__empty__push_one__false)
{
    branch instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE(!instance.empty());
}

// blocks

BOOST_AUTO_TEST_CASE(branch__blocks__default__empty)
{
    branch instance;
    BOOST_REQUIRE(instance.blocks()->empty());
}

BOOST_AUTO_TEST_CASE(branch__blocks__one__empty)
{
    branch instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.blocks()->size(), 1u);
}

// push_front

BOOST_AUTO_TEST_CASE(branch__push_front__one__success)
{
    branch_fixture instance;
    DECLARE_BLOCK(block, 0);
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE((*instance.blocks())[0] == block0);
}

BOOST_AUTO_TEST_CASE(branch__push_front__two_linked__success)
{
    branch_fixture instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header().set_previous_block_hash(block0->hash());

    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE((*instance.blocks())[0] == block0);
    BOOST_REQUIRE((*instance.blocks())[1] == block1);
}

BOOST_AUTO_TEST_CASE(branch__push_front__two_unlinked__link_failure)
{
    branch_fixture instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Ensure the blocks are not linked.
    block1->header().set_previous_block_hash(null_hash);

    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(!instance.push_front(block0));
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE((*instance.blocks())[0] == block1);
}

// top

BOOST_AUTO_TEST_CASE(branch__top__default__nullptr)
{
    branch instance;
    BOOST_REQUIRE(!instance.top());
}

BOOST_AUTO_TEST_CASE(branch__top__two_blocks__expected)
{
    branch_fixture instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header().set_previous_block_hash(block0->hash());

    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE(instance.top() == block1);
}

// top_height

BOOST_AUTO_TEST_CASE(branch__top_height__default__0)
{
    branch instance;
    BOOST_REQUIRE_EQUAL(instance.top_height(), 0u);
}

BOOST_AUTO_TEST_CASE(branch__top_height__two_blocks__expected)
{
    branch_fixture instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    static const size_t expected = 42;
    instance.set_height(expected - 2);

    // Link the blocks.
    block1->header().set_previous_block_hash(block0->hash());

    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE_EQUAL(instance.top_height(), expected);
}

// difficulty

BOOST_AUTO_TEST_CASE(branch__difficulty__default__zero)
{
    branch instance;
    BOOST_REQUIRE(instance.difficulty() == 0);
}

BOOST_AUTO_TEST_CASE(branch__difficulty__two_blocks__expected)
{
    branch instance;
    DECLARE_BLOCK(block, 0);
    DECLARE_BLOCK(block, 1);

    // Link the blocks.
    block1->header().set_previous_block_hash(block0->hash());

    BOOST_REQUIRE(instance.push_front(block1));
    BOOST_REQUIRE(instance.push_front(block0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    ///////////////////////////////////////////////////////////////////////////
    // TODO: devise value tests.
    ///////////////////////////////////////////////////////////////////////////
    BOOST_REQUIRE(instance.difficulty() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
