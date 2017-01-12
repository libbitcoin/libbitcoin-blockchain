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
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(block_entry_tests)

static const auto hash42 = hash_literal("4242424242424242424242424242424242424242424242424242424242424242");
static const auto default_block_hash = hash_literal("14508459b221041eab257d2baaa7459775ba748246c8403609eb708f0e57e74b");

// construct1/block

BOOST_AUTO_TEST_CASE(block_entry__construct1__default_block__expected)
{
    const auto block = std::make_shared<const message::block>();
    block_entry instance(block);
    BOOST_REQUIRE(instance.block() == block);
    BOOST_REQUIRE(instance.hash() == default_block_hash);
}

// construct2/hash

BOOST_AUTO_TEST_CASE(block_entry__construct2__default_block_hash__round_trips)
{
    block_entry instance(default_block_hash);
    BOOST_REQUIRE(instance.hash() == default_block_hash);
}

// parent

BOOST_AUTO_TEST_CASE(block_entry__parent__hash42__expected)
{
    const auto block = std::make_shared<message::block>();
    block->header().set_previous_block_hash(hash42);
    const auto const_block = std::const_pointer_cast<const message::block>(block);
    block_entry instance(const_block);
    BOOST_REQUIRE(instance.parent() == hash42);
}

// children

BOOST_AUTO_TEST_CASE(block_entry__children__default__empty)
{
    block_entry instance(default_block_hash);
    BOOST_REQUIRE(instance.children().empty());
}

// add_child

BOOST_AUTO_TEST_CASE(block_entry__add_child__one__single)
{
    block_entry instance(null_hash);
    const auto child = std::make_shared<const message::block>();
    instance.add_child(child);
    BOOST_REQUIRE_EQUAL(instance.children().size(), 1u);
    BOOST_REQUIRE(instance.children()[0] == child->hash());
}

BOOST_AUTO_TEST_CASE(block_entry__add_child__two__expected_order)
{
    block_entry instance(null_hash);

    const auto child1 = std::make_shared<const message::block>();
    instance.add_child(child1);

    const auto child2 = std::make_shared<message::block>();
    child2->header().set_previous_block_hash(hash42);
    const auto const_child2 = std::const_pointer_cast<const message::block>(child2);
    instance.add_child(const_child2);

    BOOST_REQUIRE_EQUAL(instance.children().size(), 2u);
    BOOST_REQUIRE(instance.children()[0] == child1->hash());
    BOOST_REQUIRE(instance.children()[1] == child2->hash());
}

////// remove_child
////
////BOOST_AUTO_TEST_CASE(block_entry__remove_child__one__sempty)
////{
////    block_entry instance(null_hash);
////    const auto child = std::make_shared<const message::block>();
////    instance.add_child(child);
////    BOOST_REQUIRE_EQUAL(instance.children().size(), 1u);
////
////    instance.remove_child(child);
////    BOOST_REQUIRE(instance.children().empty());
////}
////
////BOOST_AUTO_TEST_CASE(block_entry__remove_child__two_remove_one__expected_remaining)
////{
////    block_entry instance(null_hash);
////
////    const auto child1 = std::make_shared<const message::block>();
////    instance.add_child(child1);
////
////    const auto child2 = std::make_shared<message::block>();
////    child2->header().set_previous_block_hash(hash42);
////    const auto const_child2 = std::const_pointer_cast<const message::block>(child2);
////    instance.add_child(const_child2);
////    BOOST_REQUIRE_EQUAL(instance.children().size(), 2u);
////
////    instance.remove_child(child1);
////    BOOST_REQUIRE(instance.children()[0] == child2->hash());
////}

// equality

BOOST_AUTO_TEST_CASE(block_entry__equality__same__true)
{
    const auto block = std::make_shared<const message::block>();
    block_entry instance1(block);
    block_entry instance2(block->hash());
    BOOST_REQUIRE(instance1 == instance2);
}

BOOST_AUTO_TEST_CASE(block_entry__equality__different__false)
{
    const auto block = std::make_shared<const message::block>();
    block_entry instance1(block);
    block_entry instance2(null_hash);
    BOOST_REQUIRE(!(instance1 == instance2));
}

BOOST_AUTO_TEST_SUITE_END()
