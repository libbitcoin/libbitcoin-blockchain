/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(block_pool_tests)

// Access to protected members.
class block_pool_fixture
  : public block_pool
{
public:
    block_pool_fixture(size_t maximum_depth)
      : block_pool(maximum_depth)
    {
    }

    void prune(size_t top_height)
    {
        block_pool::prune(top_height);
    }

    bool exists(block_const_ptr candidate_block) const
    {
        return block_pool::exists(candidate_block);
    }

    block_const_ptr parent(block_const_ptr block) const
    {
        return block_pool::parent(block);
    }

    size_t maximum_depth() const
    {
        return maximum_depth_;
    }

    block_entries& blocks()
    {
        return blocks_;
    }
};

block_const_ptr make_block(uint32_t id, size_t height,
    const hash_digest& parent)
{
    const auto block = std::make_shared<const message::block>(message::block
    {
        chain::header{ id, parent, null_hash, 0, 0, 0 }, {}
    });

    block->header().validation.height = height;
    return block;
}

block_const_ptr make_block(uint32_t id, size_t height, block_const_ptr parent)
{
    return make_block(id, height, parent->hash());
}

block_const_ptr make_block(uint32_t id, size_t height)
{
    return make_block(id, height, null_hash);
}

// construct

BOOST_AUTO_TEST_CASE(block_pool__construct__zero_depth__sets__maximum_value)
{
    block_pool_fixture instance(0);
    BOOST_REQUIRE_EQUAL(instance.maximum_depth(), max_size_t);
}

BOOST_AUTO_TEST_CASE(block_pool__construct__nonzero_depth__round_trips)
{
    static const size_t expected = 42;
    block_pool_fixture instance(expected);
    BOOST_REQUIRE_EQUAL(instance.maximum_depth(), expected);
}

// add1

BOOST_AUTO_TEST_CASE(block_pool__add1__one__single)
{
    block_pool_fixture instance(0);
    static const size_t height = 42;
    const auto block1 = make_block(1, height);

    instance.add(block1);
    instance.add(block1);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);

    const auto entry = instance.blocks().right.find(height);
    BOOST_REQUIRE(entry != instance.blocks().right.end());
    BOOST_REQUIRE(entry->second.block() == block1);
    BOOST_REQUIRE_EQUAL(entry->first, height);
}

BOOST_AUTO_TEST_CASE(block_pool__add1__twice__single)
{
    block_pool instance(0);
    const auto block = std::make_shared<const message::block>();

    instance.add(block);
    instance.add(block);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
}

BOOST_AUTO_TEST_CASE(block_pool__add1__two_different_blocks_with_same_hash__first_retained)
{
    block_pool_fixture instance(0);
    static const size_t height1a = 42;
    const auto block1a = make_block(1, height1a);
    const auto block1b = make_block(1, height1a + 1u);

    // The blocks have the same hash value, so second will not be added.
    BOOST_REQUIRE(block1a->hash() == block1b->hash());

    instance.add(block1a);
    instance.add(block1b);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);

    const auto entry = instance.blocks().right.find(height1a);
    BOOST_REQUIRE(entry != instance.blocks().right.end());
    BOOST_REQUIRE(entry->second.block() == block1a);
}

BOOST_AUTO_TEST_CASE(block_pool__add1__two_distinct_hash__two)
{
    block_pool_fixture instance(0);
    static const size_t height1 = 42;
    static const size_t height2 = height1 + 1u;
    const auto block1 = make_block(1, height1);
    const auto block2 = make_block(2, height2);

    // The blocks do not have the same hash value, so both will be added.
    BOOST_REQUIRE(block1->hash() != block2->hash());

    instance.add(block1);
    instance.add(block2);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto& entry1 = instance.blocks().right.find(height1);
    BOOST_REQUIRE(entry1 != instance.blocks().right.end());
    BOOST_REQUIRE(entry1->second.block() == block1);

    const auto& entry2 = instance.blocks().right.find(height2);
    BOOST_REQUIRE(entry2 != instance.blocks().right.end());
    BOOST_REQUIRE(entry2->second.block() == block2);
}

// add2

BOOST_AUTO_TEST_CASE(block_pool__add2__empty__empty)
{
    block_pool instance(0);
    instance.add(std::make_shared<const block_const_ptr_list>());
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(block_pool__add2__distinct__expected)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    block_const_ptr_list blocks{ block1, block2 };

    // The blocks do not have the same hash value, so both will be added.
    BOOST_REQUIRE(block1->hash() != block2->hash());

    instance.add(std::make_shared<const block_const_ptr_list>(std::move(blocks)));
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto entry1 = instance.blocks().right.find(42);
    BOOST_REQUIRE(entry1 != instance.blocks().right.end());
    BOOST_REQUIRE(entry1->second.block() == block1);

    const auto& entry2 = instance.blocks().right.find(43);
    BOOST_REQUIRE(entry2 != instance.blocks().right.end());
    BOOST_REQUIRE(entry2->second.block() == block2);
}

// remove

BOOST_AUTO_TEST_CASE(block_pool__remove__empty__unchanged)
{
    block_pool instance(0);
    const auto block1 = make_block(1, 42);
    instance.add(block1);
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);

    instance.remove(std::make_shared<const block_const_ptr_list>());
    BOOST_REQUIRE_EQUAL(instance.size(), 1u);
}

BOOST_AUTO_TEST_CASE(block_pool__remove__all_distinct__empty)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    const auto block3 = make_block(2, 44);
    instance.add(block1);
    instance.add(block2);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    block_const_ptr_list path{ block1, block2 };
    instance.remove(std::make_shared<const block_const_ptr_list>(std::move(path)));
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(block_pool__remove__all_connected__empty)
{
    block_pool instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    const auto block3 = make_block(3, 44, block2);
    instance.add(block1);
    instance.add(block2);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    block_const_ptr_list path{ block1, block2, block3 };
    instance.remove(std::make_shared<const block_const_ptr_list>(std::move(path)));
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(block_pool__remove__subtree__reorganized)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    const auto block3 = make_block(3, 44, block2);
    const auto block4 = make_block(4, 45, block3);
    const auto block5 = make_block(5, 46, block4);

    // sub-branch of block2
    const auto block6 = make_block(6, 44, block2);
    const auto block7 = make_block(7, 45, block2);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    instance.add(block5);
    instance.add(block6);
    BOOST_REQUIRE_EQUAL(instance.size(), 6u);

    block_const_ptr_list path{ block1, block2, block6, block7 };
    instance.remove(std::make_shared<const block_const_ptr_list>(std::move(path)));
    BOOST_REQUIRE_EQUAL(instance.size(), 3u);

    // Entry3 is the new root block (non-zero height).
    const auto entry3 = instance.blocks().right.find(44);
    BOOST_REQUIRE(entry3 != instance.blocks().right.end());
    BOOST_REQUIRE(entry3->second.block() == block3);

    // Remaining entries are children (zero height).
    const auto children = instance.blocks().right.find(0);
    BOOST_REQUIRE(children != instance.blocks().right.end());
}

// prune

BOOST_AUTO_TEST_CASE(block_pool__prune__empty_zero_zero__empty)
{
    block_pool_fixture instance(0);
    instance.prune(0);
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
}

BOOST_AUTO_TEST_CASE(block_pool__prune__all_current__unchanged)
{
    block_pool_fixture instance(10);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    const auto block3 = make_block(3, 44);
    const auto block4 = make_block(4, 45);
    const auto block5 = make_block(5, 46);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    instance.add(block5);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);

    // Any height less than 42 (52 - 10) should be pruned.
    instance.prune(52);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);
}

BOOST_AUTO_TEST_CASE(block_pool__prune__one_expired__one_deleted)
{
    block_pool_fixture instance(10);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    const auto block3 = make_block(3, 44);
    const auto block4 = make_block(4, 45);
    const auto block5 = make_block(5, 46);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    instance.add(block5);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);

    // Any height less than 43 (53 - 10) should be pruned.
    instance.prune(53);
    BOOST_REQUIRE_EQUAL(instance.size(), 4u);
}

BOOST_AUTO_TEST_CASE(block_pool__prune__whole_branch_expired__whole_branch_deleted)
{
    block_pool_fixture instance(10);

    // branch1
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);

    // branch2
    const auto block3 = make_block(3, 44);
    const auto block4 = make_block(4, 45, block3);
    const auto block5 = make_block(5, 46, block4);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    instance.add(block5);
    BOOST_REQUIRE_EQUAL(instance.size(), 5u);

    // Any height less than 44 (54 - 10) should be pruned.
    instance.prune(54);
    BOOST_REQUIRE_EQUAL(instance.size(), 3u);
}

BOOST_AUTO_TEST_CASE(block_pool__prune__partial_branch_expired__partial_branch_deleted)
{
    block_pool_fixture instance(10);

    // branch1
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);

    // branch2
    const auto block3 = make_block(3, 44);
    const auto block4 = make_block(4, 45, block3);
    const auto block5 = make_block(5, 46, block4);

    // sub-branch of branch2
    const auto block6 = make_block(6, 45, block3);
    const auto block7 = make_block(7, 46, block6);
    const auto block8 = make_block(8, 47, block7);

    // sub-branch of branch2
    const auto block9 = make_block(9, 45, block3);
    const auto block10 = make_block(10, 46, block9);

    // sub-branch of sub-branch of branch2
    const auto block11 = make_block(11, 46, block9);
    const auto block12 = make_block(12, 47, block10);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    instance.add(block5);
    instance.add(block6);
    instance.add(block7);
    instance.add(block8);
    instance.add(block9);
    instance.add(block10);
    instance.add(block11);
    instance.add(block12);
    BOOST_REQUIRE_EQUAL(instance.size(), 12u);

    // Any height less than 46 (56 - 10) should be pruned, others replanted.
    instance.prune(56);
    BOOST_REQUIRE_EQUAL(instance.size(), 6u);

    // There are four blocks at height 46, make sure at least one exists.
    const auto entry = instance.blocks().right.find(46);
    BOOST_REQUIRE(entry != instance.blocks().right.end());

    // There are two blocks at 47 but neither is a root (not replanted).
    const auto entry8 = instance.blocks().right.find(47);
    BOOST_REQUIRE(entry8 == instance.blocks().right.end());
}

// filter

BOOST_AUTO_TEST_CASE(block_pool__filter__empty__empty)
{
    block_pool_fixture instance(0);
    const auto message = std::make_shared<message::get_data>();
    instance.filter(message);
    BOOST_REQUIRE(message->inventories().empty());
}

BOOST_AUTO_TEST_CASE(block_pool__filter__empty_filter__unchanged)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 42);
    instance.add(block1);
    instance.add(block2);
    const auto message = std::make_shared<message::get_data>();
    instance.filter(message);
    BOOST_REQUIRE(message->inventories().empty());
}

BOOST_AUTO_TEST_CASE(block_pool__filter__matched_blocks__non_blocks_and_mismatches_remain)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    const auto block3 = make_block(3, 44);
    instance.add(block1);
    instance.add(block2);
    const message::inventory_vector expected1{ message::inventory::type_id::error, block1->hash() };
    const message::inventory_vector expected2{ message::inventory::type_id::transaction, block3->hash() };
    const message::inventory_vector expected3{ message::inventory::type_id::block, block3->hash() };
    message::get_data data
    {
        expected1,
        { message::inventory::type_id::block, block1->hash() },
        expected2,
        { message::inventory::type_id::block, block2->hash() },
        { message::inventory::type_id::block, block2->hash() },
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

BOOST_AUTO_TEST_CASE(block_pool__exists__empty__false)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    BOOST_REQUIRE(!instance.exists(block1));
}

BOOST_AUTO_TEST_CASE(block_pool__exists__not_empty_mismatch__false)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    instance.add(block1);
    BOOST_REQUIRE(!instance.exists(block2));
}

BOOST_AUTO_TEST_CASE(block_pool__exists__match__true)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    instance.add(block1);
    BOOST_REQUIRE(instance.exists(block1));
}

// parent

BOOST_AUTO_TEST_CASE(block_pool__parent__empty__false)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    BOOST_REQUIRE(!instance.parent(block1));
}

BOOST_AUTO_TEST_CASE(block_pool__parent__nonempty_mismatch___false)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    instance.add(block1);
    instance.add(block2);
    BOOST_REQUIRE(!instance.parent(block2));
}

BOOST_AUTO_TEST_CASE(block_pool__parent__match___true)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    instance.add(block1);
    instance.add(block2);
    BOOST_REQUIRE(instance.parent(block2));
}

// get_path

BOOST_AUTO_TEST_CASE(block_pool__get_path__empty__self)
{
    block_pool instance(0);
    const auto block1 = make_block(1, 42);
    const auto path = instance.get_path(block1);
    BOOST_REQUIRE_EQUAL(path->size(), 1u);
    BOOST_REQUIRE(path->blocks()->front() == block1);
}

BOOST_AUTO_TEST_CASE(block_pool__get_path__exists__empty)
{
    block_pool instance(0);
    const auto block1 = make_block(1, 42);
    instance.add(block1);
    const auto path = instance.get_path(block1);
    BOOST_REQUIRE_EQUAL(path->size(), 0u);
}

BOOST_AUTO_TEST_CASE(block_pool__get_path__disconnected__self)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43);
    const auto block3 = make_block(3, 44);

    instance.add(block1);
    instance.add(block2);
    BOOST_REQUIRE_EQUAL(instance.size(), 2u);

    const auto path = instance.get_path(block3);
    BOOST_REQUIRE_EQUAL(path->size(), 1u);
    BOOST_REQUIRE(path->blocks()->front() == block3);
}

BOOST_AUTO_TEST_CASE(block_pool__get_path__connected_one_path__expected_path)
{
    block_pool_fixture instance(0);
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    const auto block3 = make_block(3, 44, block2);
    const auto block4 = make_block(4, 45, block3);
    const auto block5 = make_block(5, 46, block4);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    BOOST_REQUIRE_EQUAL(instance.size(), 4u);

    const auto path = instance.get_path(block5);
    BOOST_REQUIRE_EQUAL(path->size(), 5u);
    BOOST_REQUIRE((*path->blocks())[0] == block1);
    BOOST_REQUIRE((*path->blocks())[1] == block2);
    BOOST_REQUIRE((*path->blocks())[2] == block3);
    BOOST_REQUIRE((*path->blocks())[3] == block4);
    BOOST_REQUIRE((*path->blocks())[4] == block5);
}

BOOST_AUTO_TEST_CASE(block_pool__get_path__connected_multiple_paths__expected_path)
{
    block_pool_fixture instance(0);

    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    const auto block3 = make_block(3, 44, block2);
    const auto block4 = make_block(4, 45, block3);
    const auto block5 = make_block(5, 46, block4);

    const auto block11 = make_block(11, 420);
    const auto block12 = make_block(12, 421, block11);
    const auto block13 = make_block(13, 422, block12);
    const auto block14 = make_block(14, 423, block13);
    const auto block15 = make_block(15, 424, block14);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    BOOST_REQUIRE_EQUAL(instance.size(), 4u);

    instance.add(block11);
    instance.add(block12);
    instance.add(block13);
    instance.add(block14);
    BOOST_REQUIRE_EQUAL(instance.size(), 8u);

    const auto path1 = instance.get_path(block5);
    BOOST_REQUIRE_EQUAL(path1->size(), 5u);
    BOOST_REQUIRE((*path1->blocks())[0] == block1);
    BOOST_REQUIRE((*path1->blocks())[1] == block2);
    BOOST_REQUIRE((*path1->blocks())[2] == block3);
    BOOST_REQUIRE((*path1->blocks())[3] == block4);
    BOOST_REQUIRE((*path1->blocks())[4] == block5);

    const auto path2 = instance.get_path(block15);
    BOOST_REQUIRE_EQUAL(path2->size(), 5u);
    BOOST_REQUIRE((*path2->blocks())[0] == block11);
    BOOST_REQUIRE((*path2->blocks())[1] == block12);
    BOOST_REQUIRE((*path2->blocks())[2] == block13);
    BOOST_REQUIRE((*path2->blocks())[3] == block14);
    BOOST_REQUIRE((*path2->blocks())[4] == block15);
}

BOOST_AUTO_TEST_CASE(block_pool__get_path__connected_multiple_sub_branches__expected_path)
{
    block_pool_fixture instance(0);

    // root branch
    const auto block1 = make_block(1, 42);
    const auto block2 = make_block(2, 43, block1);
    const auto block3 = make_block(3, 44, block2);
    const auto block4 = make_block(4, 45, block3);
    const auto block5 = make_block(5, 46, block4);

    // sub-branch of block1
    const auto block11 = make_block(11, 43, block1);
    const auto block12 = make_block(12, 44, block11);

    // sub-branch of block4
    const auto block21 = make_block(21, 46, block4);
    const auto block22 = make_block(22, 47, block21);
    const auto block23 = make_block(23, 48, block22);

    instance.add(block1);
    instance.add(block2);
    instance.add(block3);
    instance.add(block4);
    instance.add(block11);
    instance.add(block21);
    instance.add(block22);
    BOOST_REQUIRE_EQUAL(instance.size(), 7u);

    const auto path1 = instance.get_path(block5);
    BOOST_REQUIRE_EQUAL(path1->size(), 5u);
    BOOST_REQUIRE((*path1->blocks())[0] == block1);
    BOOST_REQUIRE((*path1->blocks())[1] == block2);
    BOOST_REQUIRE((*path1->blocks())[2] == block3);
    BOOST_REQUIRE((*path1->blocks())[3] == block4);
    BOOST_REQUIRE((*path1->blocks())[4] == block5);

    const auto path2 = instance.get_path(block12);
    BOOST_REQUIRE_EQUAL(path2->size(), 3u);
    BOOST_REQUIRE((*path2->blocks())[0] == block1);
    BOOST_REQUIRE((*path2->blocks())[1] == block11);
    BOOST_REQUIRE((*path2->blocks())[2] == block12);

    const auto path3 = instance.get_path(block23);
    BOOST_REQUIRE_EQUAL(path3->size(), 7u);
    BOOST_REQUIRE((*path3->blocks())[0] == block1);
    BOOST_REQUIRE((*path3->blocks())[1] == block2);
    BOOST_REQUIRE((*path3->blocks())[2] == block3);
    BOOST_REQUIRE((*path3->blocks())[3] == block4);
    BOOST_REQUIRE((*path3->blocks())[4] == block21);
    BOOST_REQUIRE((*path3->blocks())[5] == block22);
    BOOST_REQUIRE((*path3->blocks())[6] == block23);
}

BOOST_AUTO_TEST_SUITE_END()
