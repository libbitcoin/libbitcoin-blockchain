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

#include <utility>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(header_pool_tests)

// Access to protected members.
class header_pool_fixture
  : public header_pool
{
public:
    header_pool_fixture(size_t maximum_depth)
      : header_pool(maximum_depth)
    {
    }

    size_t maximum_depth() const
    {
        return maximum_depth_;
    }

    header_entries& headers()
    {
        return headers_;
    }
};

header_const_ptr make_header(uint32_t id, const hash_digest& parent)
{
    const auto header = std::make_shared<const message::header>(chain::header
    {
        id, parent, null_hash, 0, 0, 0
    });

    return header;
}

header_const_ptr make_header(uint32_t id,header_const_ptr parent)
{
    return make_header(id, parent->hash());
}

header_const_ptr make_header(uint32_t id)
{
    return make_header(id, null_hash);
}

// construct

BOOST_AUTO_TEST_CASE(header_pool__construct__zero_depth__sets__maximum_value)
{
    header_pool_fixture instance(0);
    BOOST_REQUIRE_EQUAL(instance.maximum_depth(), max_size_t);
}

BOOST_AUTO_TEST_CASE(header_pool__construct__nonzero_depth__round_trips)
{
    static const size_t expected = 42;
    header_pool_fixture instance(expected);
    BOOST_REQUIRE_EQUAL(instance.maximum_depth(), expected);
}

// add1

BOOST_AUTO_TEST_CASE(header_pool__add1__one__single)
{
    header_pool_fixture instance(0);
    static const size_t height = 42;
    const auto header1 = make_header(1);

    instance.add(header1, height);
    instance.add(header1, height);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);

    const auto entry = instance.headers().right.find(height);
    BOOST_REQUIRE(entry != instance.headers().right.end());
    BOOST_REQUIRE(entry->second.header() == header1);
    BOOST_REQUIRE_EQUAL(entry->first, height);
}

BOOST_AUTO_TEST_CASE(header_pool__add1__twice__single)
{
    header_pool instance(0);
    const auto header = std::make_shared<const message::header>();

    instance.add(header, 0);
    instance.add(header, 0);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
}

BOOST_AUTO_TEST_CASE(header_pool__add1__two_different_headers_with_same_hash__first_retained)
{
    header_pool_fixture instance(0);
    static const size_t height1a = 42;
    const auto header1a = make_header(1);
    const auto header1b = make_header(1);

    // The headers have the same hash value, so second will not be added.
    BOOST_REQUIRE(header1a->hash() == header1b->hash());

    instance.add(header1a, height1a);
    instance.add(header1b, height1a + 1);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);

    const auto entry = instance.headers().right.find(height1a);
    BOOST_REQUIRE(entry != instance.headers().right.end());
    BOOST_REQUIRE(entry->second.header() == header1a);
}

BOOST_AUTO_TEST_CASE(header_pool__add1__two_distinct_hash__two)
{
    header_pool_fixture instance(0);
    static const size_t height1 = 42;
    static const size_t height2 = height1 + 1;
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);

    // The headers do not have the same hash value, so both will be added.
    BOOST_REQUIRE(header1->hash() != header2->hash());

    instance.add(header1, height1);
    instance.add(header2, height2);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto& entry1 = instance.headers().right.find(height1);
    BOOST_REQUIRE(entry1 != instance.headers().right.end());
    BOOST_REQUIRE(entry1->second.header() == header1);

    const auto& entry2 = instance.headers().right.find(height2);
    BOOST_REQUIRE(entry2 != instance.headers().right.end());
    BOOST_REQUIRE(entry2->second.header() == header2);
}

// add2

BOOST_AUTO_TEST_CASE(header_pool__add2__empty__empty)
{
    header_pool instance(0);
    instance.add(std::make_shared<const header_const_ptr_list>(), 0);
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_pool__add2__distinct__expected)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    header_const_ptr_list headers{ header1, header2 };

    // The headers do not have the same hash value, so both will be added.
    BOOST_REQUIRE(header1->hash() != header2->hash());

    instance.add(std::make_shared<const header_const_ptr_list>(std::move(headers)), 42);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto entry1 = instance.headers().right.find(42);
    BOOST_REQUIRE(entry1 != instance.headers().right.end());
    BOOST_REQUIRE(entry1->second.header() == header1);

    const auto& entry2 = instance.headers().right.find(43);
    BOOST_REQUIRE(entry2 != instance.headers().right.end());
    BOOST_REQUIRE(entry2->second.header() == header2);
}

// remove

BOOST_AUTO_TEST_CASE(header_pool__remove__empty__unchanged)
{
    header_pool instance(0);
    const auto header1 = make_header(1);
    instance.add(header1, 42);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);

    instance.remove(std::make_shared<const header_const_ptr_list>());
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
}

BOOST_AUTO_TEST_CASE(header_pool__remove__all_distinct__empty)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    const auto header3 = make_header(2);
    instance.add(header1, 42);
    instance.add(header2, 43);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    header_const_ptr_list path{ header1, header2 };
    instance.remove(std::make_shared<const header_const_ptr_list>(std::move(path)));
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_pool__remove__all_connected__empty)
{
    header_pool instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);
    const auto header3 = make_header(3, header2);
    instance.add(header1, 42);
    instance.add(header2, 42);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    header_const_ptr_list path{ header1, header2, header3 };
    instance.remove(std::make_shared<const header_const_ptr_list>(std::move(path)));
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_pool__remove__subtree__reorganized)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);
    const auto header3 = make_header(3, header2);
    const auto header4 = make_header(4, header3);
    const auto header5 = make_header(5, header4);

    // sub-header_branch of header2
    const auto header6 = make_header(6, header2);
    const auto header7 = make_header(7, header2);

    instance.add(header1, 42);
    instance.add(header2, 43);
    instance.add(header3, 44);
    instance.add(header4, 45);
    instance.add(header5, 46);
    instance.add(header6, 44);
    BOOST_REQUIRE_EQUAL(instance.size(), 6u);

    header_const_ptr_list path{ header1, header2, header6, header7 };
    instance.remove(std::make_shared<const header_const_ptr_list>(std::move(path)));
    BOOST_REQUIRE_EQUAL(instance.size(), 3u);

    // Entry3 is the new root header (non-zero height).
    const auto entry3 = instance.headers().right.find(44);
    BOOST_REQUIRE(entry3 != instance.headers().right.end());
    BOOST_REQUIRE(entry3->second.header() == header3);

    // Remaining entries are children (zero height).
    const auto children = instance.headers().right.find(0);
    BOOST_REQUIRE(children != instance.headers().right.end());
}

// prune

BOOST_AUTO_TEST_CASE(header_pool__prune__empty_zero_zero__empty)
{
    header_pool_fixture instance(0);
    instance.prune(0);
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_pool__prune__all_current__unchanged)
{
    header_pool_fixture instance(10);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    const auto header3 = make_header(3);
    const auto header4 = make_header(4);
    const auto header5 = make_header(5);

    instance.add(header1, 42);
    instance.add(header2, 43);
    instance.add(header3, 44);
    instance.add(header4, 45);
    instance.add(header5, 46);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);

    // Any height less than 42 (52 - 10) should be pruned.
    instance.prune(52);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);
}

BOOST_AUTO_TEST_CASE(header_pool__prune__one_expired__one_deleted)
{
    header_pool_fixture instance(10);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    const auto header3 = make_header(3);
    const auto header4 = make_header(4);
    const auto header5 = make_header(5);

    instance.add(header1, 42);
    instance.add(header2, 43);
    instance.add(header3, 44);
    instance.add(header4, 45);
    instance.add(header5, 46);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);

    // Any height less than 43 (53 - 10) should be pruned.
    instance.prune(53);
    BOOST_REQUIRE_EQUAL(instance.size(), 4u);
}

BOOST_AUTO_TEST_CASE(header_pool__prune__whole_header_branch_expired__whole_header_branch_deleted)
{
    header_pool_fixture instance(10);

    // header_branch1
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);

    // header_branch2
    const auto header3 = make_header(3);
    const auto header4 = make_header(4, header3);
    const auto header5 = make_header(5, header4);

    instance.add(header1, 42);
    instance.add(header2, 43);
    instance.add(header3, 44);
    instance.add(header4, 45);
    instance.add(header5, 46);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);

    // Any height less than 44 (54 - 10) should be pruned.
    instance.prune(54);
    BOOST_REQUIRE_EQUAL(instance.size(), 3u);
}

BOOST_AUTO_TEST_CASE(header_pool__prune__partial_header_branch_expired__partial_header_branch_deleted)
{
    header_pool_fixture instance(10);

    // header_branch1
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);

    // header_branch2
    const auto header3 = make_header(3);
    const auto header4 = make_header(4, header3);
    const auto header5 = make_header(5, header4);

    // sub-header_branch of header_branch2
    const auto header6 = make_header(6, header3);
    const auto header7 = make_header(7, header6);
    const auto header8 = make_header(8, header7);

    // sub-header_branch of header_branch2
    const auto header9 = make_header(9, header3);
    const auto header10 = make_header(10, header9);

    // sub-header_branch of sub-header_branch of header_branch2
    const auto header11 = make_header(11, header9);
    const auto header12 = make_header(12, header10);

    instance.add(header1, 42);
    instance.add(header2, 43);
    instance.add(header3, 44);
    instance.add(header4, 45);
    instance.add(header5, 46);
    instance.add(header6, 45);
    instance.add(header7, 46);
    instance.add(header8, 47);
    instance.add(header9, 45);
    instance.add(header10, 46);
    instance.add(header11, 46);
    instance.add(header12, 47);
    BOOST_REQUIRE_EQUAL(instance.size(), 12u);

    // Any height less than 46 (56 - 10) should be pruned, others replanted.
    instance.prune(56);
    BOOST_REQUIRE_EQUAL(instance.size(), 6u);

    // There are four headers at height 46, make sure at least one exists.
    const auto entry = instance.headers().right.find(46);
    BOOST_REQUIRE(entry != instance.headers().right.end());

    // There are two headers at 47 but neither is a root (not replanted).
    const auto entry8 = instance.headers().right.find(47);
    BOOST_REQUIRE(entry8 == instance.headers().right.end());
}

// filter

BOOST_AUTO_TEST_CASE(header_pool__filter__empty__empty)
{
    header_pool_fixture instance(0);
    const auto message = std::make_shared<message::get_data>();
    instance.filter(message);
    BOOST_REQUIRE(message->inventories().empty());
}

BOOST_AUTO_TEST_CASE(header_pool__filter__empty_filter__unchanged)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    instance.add(header1, 42);
    instance.add(header2, 42);
    const auto message = std::make_shared<message::get_data>();
    instance.filter(message);
    BOOST_REQUIRE(message->inventories().empty());
}

BOOST_AUTO_TEST_CASE(header_pool__filter__matched_headers__non_headers_and_mismatches_remain)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    const auto header3 = make_header(3);
    instance.add(header1, 42);
    instance.add(header2, 43);
    const message::inventory_vector expected1{ message::inventory::type_id::error, header1->hash() };
    const message::inventory_vector expected2{ message::inventory::type_id::transaction, header3->hash() };
    const message::inventory_vector expected3{ message::inventory::type_id::block, header3->hash() };
    message::get_data data
    {
        expected1,
        { message::inventory::type_id::block, header1->hash() },
        expected2,
        { message::inventory::type_id::block, header2->hash() },
        { message::inventory::type_id::block, header2->hash() },
        expected3
    };
    const auto message = std::make_shared<message::get_data>(std::move(data));
    instance.filter(message);
    BOOST_REQUIRE_EQUAL(message->inventories().size(), 3u);
    BOOST_REQUIRE(message->inventories()[0] == expected1);
    BOOST_REQUIRE(message->inventories()[1] == expected2);
    BOOST_REQUIRE(message->inventories()[2] == expected3);
}

// exists

BOOST_AUTO_TEST_CASE(header_pool__exists__empty__false)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    BOOST_REQUIRE(!instance.exists(header1));
}

BOOST_AUTO_TEST_CASE(header_pool__exists__not_empty_mismatch__false)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);
    instance.add(header1, 42);
    BOOST_REQUIRE(!instance.exists(header2));
}

BOOST_AUTO_TEST_CASE(header_pool__exists__match__true)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    instance.add(header1, 42);
    BOOST_REQUIRE(instance.exists(header1));
}

// get_branch

BOOST_AUTO_TEST_CASE(header_pool__get_branch__empty__self_default_height)
{
    header_pool instance(0);
    const auto header1 = make_header(1);
    const auto path = instance.get_branch(header1);
    BOOST_REQUIRE_EQUAL(path->size(), 1u);
    BOOST_REQUIRE_EQUAL(path->fork_height(), max_size_t);
    BOOST_REQUIRE(path->headers()->front() == header1);
}

BOOST_AUTO_TEST_CASE(header_pool__get_branch__exists__empty)
{
    header_pool instance(0);
    const auto header1 = make_header(1);
    instance.add(header1, 42);
    const auto path = instance.get_branch(header1);
    BOOST_REQUIRE_EQUAL(path->size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_pool__get_branch__disconnected__self_default_height)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2);
    const auto header3 = make_header(3);

    instance.add(header1, 42);
    instance.add(header2, 43);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto path = instance.get_branch(header3);
    BOOST_REQUIRE_EQUAL(path->size(), 1u);
    BOOST_REQUIRE_EQUAL(path->fork_height(), max_size_t);
    BOOST_REQUIRE(path->headers()->front() == header3);
}

BOOST_AUTO_TEST_CASE(header_pool__get_branch__connected_one_path__expected_path_and_height)
{
    header_pool_fixture instance(0);
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);
    const auto header3 = make_header(3, header2);
    const auto header4 = make_header(4, header3);
    const auto header5 = make_header(5, header4);

    const auto fork_point = 41u;
    instance.add(header1, fork_point + 1);
    instance.add(header2, fork_point + 2);
    instance.add(header3, fork_point + 3);
    instance.add(header4, fork_point + 4);
    BOOST_REQUIRE_EQUAL(instance.size(), 4u);

    const auto path = instance.get_branch(header5);
    BOOST_REQUIRE_EQUAL(path->size(), 5u);
    BOOST_REQUIRE_EQUAL(path->fork_height(), fork_point);
    BOOST_REQUIRE((*path->headers())[0] == header1);
    BOOST_REQUIRE((*path->headers())[1] == header2);
    BOOST_REQUIRE((*path->headers())[2] == header3);
    BOOST_REQUIRE((*path->headers())[3] == header4);
    BOOST_REQUIRE((*path->headers())[4] == header5);
}

BOOST_AUTO_TEST_CASE(header_pool__get_branch__connected_multiple_paths__expected_paths)
{
    header_pool_fixture instance(0);

    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);
    const auto header3 = make_header(3, header2);
    const auto header4 = make_header(4, header3);
    const auto header5 = make_header(5, header4);

    const auto header11 = make_header(11);
    const auto header12 = make_header(12, header11);
    const auto header13 = make_header(13, header12);
    const auto header14 = make_header(14, header13);
    const auto header15 = make_header(15, header14);

    const auto fork_point1 = 41u;
    instance.add(header1, fork_point1 + 1);
    instance.add(header2, fork_point1 + 2);
    instance.add(header3, fork_point1 + 3);
    instance.add(header4, fork_point1 + 4);
    BOOST_REQUIRE_EQUAL(instance.size(), 4u);

    const auto fork_point2 = 419u;
    instance.add(header11, fork_point2 + 1);
    instance.add(header12, fork_point2 + 2);
    instance.add(header13, fork_point2 + 3);
    instance.add(header14, fork_point2 + 4);
    BOOST_REQUIRE_EQUAL(instance.size(), 8u);

    const auto path1 = instance.get_branch(header5);
    BOOST_REQUIRE_EQUAL(path1->size(), 5u);
    BOOST_REQUIRE_EQUAL(path1->fork_height(), fork_point1);
    BOOST_REQUIRE((*path1->headers())[0] == header1);
    BOOST_REQUIRE((*path1->headers())[1] == header2);
    BOOST_REQUIRE((*path1->headers())[2] == header3);
    BOOST_REQUIRE((*path1->headers())[3] == header4);
    BOOST_REQUIRE((*path1->headers())[4] == header5);

    const auto path2 = instance.get_branch(header15);
    BOOST_REQUIRE_EQUAL(path2->size(), 5u);
    BOOST_REQUIRE_EQUAL(path2->fork_height(), fork_point2);
    BOOST_REQUIRE((*path2->headers())[0] == header11);
    BOOST_REQUIRE((*path2->headers())[1] == header12);
    BOOST_REQUIRE((*path2->headers())[2] == header13);
    BOOST_REQUIRE((*path2->headers())[3] == header14);
    BOOST_REQUIRE((*path2->headers())[4] == header15);
}

BOOST_AUTO_TEST_CASE(header_pool__get_branch__connected_multiple_sub_header_branches__expected_paths_and_heights)
{
    header_pool_fixture instance(0);

    // root header_branch
    const auto header1 = make_header(1);
    const auto header2 = make_header(2, header1);
    const auto header3 = make_header(3, header2);
    const auto header4 = make_header(4, header3);
    const auto header5 = make_header(5, header4);

    // sub-header_branch of header1
    const auto header11 = make_header(11, header1);
    const auto header12 = make_header(12, header11);

    // sub-header_branch of header4
    const auto header21 = make_header(21, header4);
    const auto header22 = make_header(22, header21);
    const auto header23 = make_header(23, header22);

    const auto fork_point = 41u;
    instance.add(header1, fork_point + 1);
    instance.add(header2, fork_point + 2);
    instance.add(header3, fork_point + 3);
    instance.add(header4, fork_point + 4);
    instance.add(header11, fork_point + 5);
    instance.add(header21, fork_point + 6);
    instance.add(header22, fork_point + 7);
    BOOST_REQUIRE_EQUAL(instance.size(), 7u);

    const auto path1 = instance.get_branch(header5);
    BOOST_REQUIRE_EQUAL(path1->size(), 5u);
    BOOST_REQUIRE_EQUAL(path1->fork_height(), fork_point);
    BOOST_REQUIRE((*path1->headers())[0] == header1);
    BOOST_REQUIRE((*path1->headers())[1] == header2);
    BOOST_REQUIRE((*path1->headers())[2] == header3);
    BOOST_REQUIRE((*path1->headers())[3] == header4);
    BOOST_REQUIRE((*path1->headers())[4] == header5);

    const auto path2 = instance.get_branch(header12);
    BOOST_REQUIRE_EQUAL(path2->size(), 3u);
    BOOST_REQUIRE_EQUAL(path2->fork_height(), fork_point);
    BOOST_REQUIRE((*path2->headers())[0] == header1);
    BOOST_REQUIRE((*path2->headers())[1] == header11);
    BOOST_REQUIRE((*path2->headers())[2] == header12);

    const auto path3 = instance.get_branch(header23);
    BOOST_REQUIRE_EQUAL(path3->size(), 7u);
    BOOST_REQUIRE_EQUAL(path3->fork_height(), fork_point);
    BOOST_REQUIRE((*path3->headers())[0] == header1);
    BOOST_REQUIRE((*path3->headers())[1] == header2);
    BOOST_REQUIRE((*path3->headers())[2] == header3);
    BOOST_REQUIRE((*path3->headers())[3] == header4);
    BOOST_REQUIRE((*path3->headers())[4] == header21);
    BOOST_REQUIRE((*path3->headers())[5] == header22);
    BOOST_REQUIRE((*path3->headers())[6] == header23);
}

BOOST_AUTO_TEST_SUITE_END()
