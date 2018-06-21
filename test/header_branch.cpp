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
using namespace bc::message;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(header_branch_tests)

#define DECLARE_HEADER(name, number) \
    const auto name##number = std::make_shared<header>(bc::settings()); \
    name##number->set_bits(number);

// Access to protected members.
class header_branch_fixture
  : public header_branch
{
public:
    size_t index_of(size_t height) const
    {
        return header_branch::index_of(height);
    }

    size_t height_at(size_t index) const
    {
        return header_branch::height_at(index);
    }
};

// hash

BOOST_AUTO_TEST_CASE(header_branch__hash__default__null_hash)
{
    header_branch instance;
    BOOST_REQUIRE(instance.hash() == null_hash);
}

BOOST_AUTO_TEST_CASE(header_branch__hash__one_header__only_previous_block_hash)
{
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    const auto expected = header0->hash();
    header1->set_previous_block_hash(expected);

    header_branch instance;
    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(instance.hash() == expected);
}

BOOST_AUTO_TEST_CASE(header_branch__hash__two_headers__first_previous_block_hash)
{
    header_branch instance;
    DECLARE_HEADER(top, 42);
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    // Link the headers.
    const auto expected = top42->hash();
    header0->set_previous_block_hash(expected);
    header1->set_previous_block_hash(header0->hash());

    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE(instance.hash() == expected);
}

// height/set_height

BOOST_AUTO_TEST_CASE(header_branch__height__default__max_size_t)
{
    header_branch instance;
    BOOST_REQUIRE_EQUAL(instance.height(), max_size_t);
}

BOOST_AUTO_TEST_CASE(header_branch__set_height__round_trip__unchanged)
{
    static const size_t expected = 42;
    header_branch instance;
    instance.set_height(expected);
    BOOST_REQUIRE_EQUAL(instance.height(), expected);
}

// index_of

BOOST_AUTO_TEST_CASE(header_branch__index_of__one__zero)
{
    header_branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.index_of(1), 0u);
}

BOOST_AUTO_TEST_CASE(header_branch__index_of__two__one)
{
    header_branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.index_of(2), 1u);
}

BOOST_AUTO_TEST_CASE(header_branch__index_of__value__expected)
{
    header_branch_fixture instance;
    instance.set_height(42);
    BOOST_REQUIRE_EQUAL(instance.index_of(53), 10u);
}

// height_at

BOOST_AUTO_TEST_CASE(header_branch__height_at__zero__one)
{
    header_branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.height_at(0), 1u);
}

BOOST_AUTO_TEST_CASE(header_branch__height_at__one__two)
{
    header_branch_fixture instance;
    instance.set_height(0);
    BOOST_REQUIRE_EQUAL(instance.height_at(1), 2u);
}

BOOST_AUTO_TEST_CASE(header_branch__height_at__value__expected)
{
    header_branch_fixture instance;
    instance.set_height(42);
    BOOST_REQUIRE_EQUAL(instance.height_at(10), 53u);
}

// size

BOOST_AUTO_TEST_CASE(header_branch__size__empty__zero)
{
    header_branch instance;
    BOOST_REQUIRE_EQUAL(instance.size(), 0);
}

// empty

BOOST_AUTO_TEST_CASE(header_branch__empty__default__true)
{
    header_branch instance;
    BOOST_REQUIRE(instance.empty());
}

BOOST_AUTO_TEST_CASE(header_branch__empty__push_one__false)
{
    header_branch instance;
    DECLARE_HEADER(header, 0);
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE(!instance.empty());
}

// headers

BOOST_AUTO_TEST_CASE(header_branch__headers__default__empty)
{
    header_branch instance;
    BOOST_REQUIRE(instance.headers()->empty());
}

BOOST_AUTO_TEST_CASE(header_branch__headers__one__empty)
{
    header_branch instance;
    DECLARE_HEADER(header, 0);
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.headers()->size(), 1u);
}

// push_front

BOOST_AUTO_TEST_CASE(header_branch__push_front__one__success)
{
    header_branch_fixture instance;
    DECLARE_HEADER(header, 0);
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE(!instance.empty());
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE((*instance.headers())[0] == header0);
}

BOOST_AUTO_TEST_CASE(header_branch__push_front__two_linked__success)
{
    header_branch_fixture instance;
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    // Link the headers.
    header1->set_previous_block_hash(header0->hash());

    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE((*instance.headers())[0] == header0);
    BOOST_REQUIRE((*instance.headers())[1] == header1);
}

BOOST_AUTO_TEST_CASE(header_branch__push_front__two_unlinked__link_failure)
{
    header_branch_fixture instance;
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    // Ensure the headers are not linked.
    header1->set_previous_block_hash(null_hash);

    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(!instance.push(header0));
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
    BOOST_REQUIRE((*instance.headers())[0] == header1);
}

// top

BOOST_AUTO_TEST_CASE(header_branch__top__default__nullptr)
{
    header_branch instance;
    BOOST_REQUIRE(!instance.top());
}

BOOST_AUTO_TEST_CASE(header_branch__top__two_headers__expected)
{
    header_branch_fixture instance;
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    // Link the headers.
    header1->set_previous_block_hash(header0->hash());

    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE(instance.top() == header1);
}

// top_height

BOOST_AUTO_TEST_CASE(header_branch__top_height__default__max_size_t)
{
    header_branch instance;
    BOOST_REQUIRE_EQUAL(instance.top_height(), max_size_t);
}

BOOST_AUTO_TEST_CASE(header_branch__top_height__two_headers__expected)
{
    header_branch_fixture instance;
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    static const size_t expected = 42;
    instance.set_height(expected - 2);

    // Link the headers.
    header1->set_previous_block_hash(header0->hash());

    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);
    BOOST_REQUIRE_EQUAL(instance.top_height(), expected);
}

// work

BOOST_AUTO_TEST_CASE(header_branch__work__default__zero)
{
    header_branch instance;
    BOOST_REQUIRE(instance.work() == 0);
}

BOOST_AUTO_TEST_CASE(header_branch__work__two_headers__expected)
{
    header_branch instance;
    DECLARE_HEADER(header, 0);
    DECLARE_HEADER(header, 1);

    // Link the headers.
    header1->set_previous_block_hash(header0->hash());

    BOOST_REQUIRE(instance.push(header1));
    BOOST_REQUIRE(instance.push(header0));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    ///////////////////////////////////////////////////////////////////////////
    // TODO: devise value tests.
    ///////////////////////////////////////////////////////////////////////////
    BOOST_REQUIRE(instance.work() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
