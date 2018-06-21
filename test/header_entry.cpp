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

#include <memory>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(header_entry_tests)

static const auto hash42 = hash_literal("4242424242424242424242424242424242424242424242424242424242424242");
static const auto default_header_hash = hash_literal("14508459b221041eab257d2baaa7459775ba748246c8403609eb708f0e57e74b");

// construct1/header

BOOST_AUTO_TEST_CASE(header_entry__construct1__default_header__expected)
{
    const auto header = std::make_shared<const message::header>(bc::settings());
    header_entry instance(header, 0);
    BOOST_REQUIRE(instance.header() == header);
    BOOST_REQUIRE(instance.hash() == default_header_hash);
}

// construct2/hash

BOOST_AUTO_TEST_CASE(header_entry__construct2__default_header_hash__round_trips)
{
    header_entry instance(default_header_hash);
    BOOST_REQUIRE(instance.hash() == default_header_hash);
}

// parent

BOOST_AUTO_TEST_CASE(header_entry__parent__hash42__expected)
{
    const auto header = std::make_shared<message::header>(bc::settings());
    header->set_previous_block_hash(hash42);
    header_entry instance(header, 0);
    BOOST_REQUIRE(instance.parent() == hash42);
}

// children

BOOST_AUTO_TEST_CASE(header_entry__children__default__empty)
{
    header_entry instance(default_header_hash);
    BOOST_REQUIRE(instance.children().empty());
}

// add_child

BOOST_AUTO_TEST_CASE(header_entry__add_child__one__single)
{
    header_entry instance(null_hash);
    const auto child = std::make_shared<const message::header>(bc::settings());
    instance.add_child(child);
    BOOST_REQUIRE_EQUAL(instance.children().size(), 1u);
    BOOST_REQUIRE(instance.children()[0] == child->hash());
}

BOOST_AUTO_TEST_CASE(header_entry__add_child__two__expected_order)
{
    header_entry instance(null_hash);

    const auto child1 = std::make_shared<const message::header>(bc::settings());
    instance.add_child(child1);

    const auto child2 = std::make_shared<message::header>(bc::settings());
    child2->set_previous_block_hash(hash42);
    instance.add_child(child2);

    BOOST_REQUIRE_EQUAL(instance.children().size(), 2u);
    BOOST_REQUIRE(instance.children()[0] == child1->hash());
    BOOST_REQUIRE(instance.children()[1] == child2->hash());
}

// equality

BOOST_AUTO_TEST_CASE(header_entry__equality__same__true)
{
    const auto header = std::make_shared<const message::header>(bc::settings());
    header_entry instance1(header, 0);
    header_entry instance2(header->hash());
    BOOST_REQUIRE(instance1 == instance2);
}

BOOST_AUTO_TEST_CASE(header_entry__equality__different__false)
{
    const auto header = std::make_shared<const message::header>(bc::settings());
    header_entry instance1(header, 0);
    header_entry instance2(null_hash);
    BOOST_REQUIRE(!(instance1 == instance2));
}

BOOST_AUTO_TEST_CASE(header_entry__height__default__zero)
{
    const auto header = std::make_shared<const message::header>(bc::settings());
    header_entry instance(hash_digest{});
    BOOST_REQUIRE_EQUAL(instance.height(), 0);
}

BOOST_AUTO_TEST_CASE(header_entry__height__nonzero__expected)
{
    const auto header = std::make_shared<const message::header>(bc::settings());
    const auto expected = 42u;
    header_entry instance(header, expected);
    BOOST_REQUIRE_EQUAL(instance.height(), expected);
}

BOOST_AUTO_TEST_SUITE_END()
