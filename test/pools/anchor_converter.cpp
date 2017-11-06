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
#include "utilities.hpp"

using namespace bc;
using namespace bc::blockchain;
using namespace bc::blockchain::test::pools;

static void insert_pool(transaction_pool_state& state,
    transaction_entry::ptr entry, transaction_pool_state::priority value)
{
    state.pool.insert({ entry, value });
}

static void insert_block_template(transaction_pool_state& state,
    transaction_entry::ptr entry, transaction_pool_state::priority value)
{
    state.pool.insert({ entry, value });
    state.block_template.insert({ entry, value });
    state.block_template_bytes += entry->size();
    state.block_template_sigops += entry->sigops();
}

static bool in_pool(transaction_pool_state& state, transaction_entry::ptr entry)
{
    return (state.pool.left.find(entry) != state.pool.left.end());
}

BOOST_AUTO_TEST_SUITE(anchor_converter_tests)

BOOST_AUTO_TEST_CASE(anchor_converter__add_bounds__multiple_differing_values__success)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    auto tx_1 = utilities::get_const_tx(1u, 0u);
    auto tx_2 = utilities::get_const_tx(2u, 0u);
    auto tx_3 = utilities::get_const_tx(3u, 0u);
    auto tx_4 = utilities::get_const_tx(4u, 0u);

    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_1));
    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_2));
    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_3));
    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_4));
}

BOOST_AUTO_TEST_CASE(anchor_converter__add_bounds__multiple_identical_values__success)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    auto tx_1 = utilities::get_const_tx(1u, 0u);
    auto tx_2 = utilities::get_const_tx(1u, 0u);

    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_1));
    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_2));
    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_1));
    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx_2));
}

BOOST_AUTO_TEST_CASE(anchor_converter__within_bounds__check_without_add__returns_false)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    auto tx = utilities::get_const_tx(12357u, 0u);

    BOOST_REQUIRE_EQUAL(false, converter.within_bounds(tx->hash()));
}

BOOST_AUTO_TEST_CASE(anchor_converter__within_bounds__check_after_add__returns_true)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    auto tx = utilities::get_const_tx(12357u, 0u);

    BOOST_REQUIRE_NO_THROW(converter.add_bounds(tx));
    BOOST_REQUIRE_EQUAL(true, converter.within_bounds(tx->hash()));
}

BOOST_AUTO_TEST_CASE(anchor_converter__demote__empty__nop_returns_zero)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);
    auto result = converter.demote();

    BOOST_REQUIRE_EQUAL(0.0, result);
}

BOOST_AUTO_TEST_CASE(anchor_converter__demote__anchor_only_graph_enqueued_anchor__removes_pool_returns_zero)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto entry = utilities::get_entry(state, 1u, 0u);
    insert_pool(pool_state, entry, 1.0);
    BOOST_REQUIRE(in_pool(pool_state, entry));
    converter.enqueue(entry);

    auto result = converter.demote();

    BOOST_REQUIRE_EQUAL(0.0, result);
    BOOST_REQUIRE(!in_pool(pool_state, entry));
}

BOOST_AUTO_TEST_CASE(anchor_converter__demote__enqueued_childless_non_anchor_with_anchor_parents__removes_graph_returns_zero)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto non_anchor = utilities::get_entry(state, 1u, 0u);
    auto parent_1 = utilities::get_entry(state, 2u, 0u);
    auto parent_2 = utilities::get_entry(state, 3u, 0u);
    auto parent_3 = utilities::get_entry(state, 4u, 0u);

    utilities::connect(parent_1, non_anchor, 0u);
    utilities::connect(parent_2, non_anchor, 0u);
    utilities::connect(parent_3, non_anchor, 0u);

    insert_pool(pool_state, non_anchor, 1.0);
    insert_pool(pool_state, parent_1, 2.0);
    insert_pool(pool_state, parent_2, 3.0);
    insert_pool(pool_state, parent_3, 4.0);

    BOOST_REQUIRE(in_pool(pool_state, non_anchor));
    BOOST_REQUIRE(in_pool(pool_state, parent_1));
    BOOST_REQUIRE(in_pool(pool_state, parent_2));
    BOOST_REQUIRE(in_pool(pool_state, parent_3));

    converter.enqueue(non_anchor);

    auto result = converter.demote();

    BOOST_REQUIRE_EQUAL(0.0, result);
    BOOST_REQUIRE(!in_pool(pool_state, non_anchor));
    BOOST_REQUIRE(!in_pool(pool_state, parent_1));
    BOOST_REQUIRE(!in_pool(pool_state, parent_2));
    BOOST_REQUIRE(!in_pool(pool_state, parent_3));

    utilities::sever({ parent_1, parent_2, parent_3, non_anchor });
}

BOOST_AUTO_TEST_CASE(anchor_converter__demote__enqueued_childless_non_anchor_with_anchor_parents_in_template__removes_graph_returns_non_anchor_priority)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto non_anchor = utilities::get_entry(state, 1u, 0u);
    auto parent_1 = utilities::get_entry(state, 2u, 0u);
    auto parent_2 = utilities::get_entry(state, 3u, 0u);
    auto parent_3 = utilities::get_entry(state, 4u, 0u);

    utilities::connect(parent_1, non_anchor, 0u);
    utilities::connect(parent_2, non_anchor, 0u);
    utilities::connect(parent_3, non_anchor, 0u);

    insert_block_template(pool_state, non_anchor, 1.0);
    insert_pool(pool_state, parent_1, 2.0);
    insert_pool(pool_state, parent_2, 3.0);
    insert_pool(pool_state, parent_3, 4.0);

    BOOST_REQUIRE(in_pool(pool_state, non_anchor));
    BOOST_REQUIRE(in_pool(pool_state, parent_1));
    BOOST_REQUIRE(in_pool(pool_state, parent_2));
    BOOST_REQUIRE(in_pool(pool_state, parent_3));
    BOOST_REQUIRE(pool_state.block_template_bytes == non_anchor->size());
    BOOST_REQUIRE(pool_state.block_template_sigops == non_anchor->sigops());

    converter.enqueue(non_anchor);

    auto result = converter.demote();

    BOOST_REQUIRE_EQUAL(1.0, result);
    BOOST_REQUIRE(!in_pool(pool_state, non_anchor));
    BOOST_REQUIRE(!in_pool(pool_state, parent_1));
    BOOST_REQUIRE(!in_pool(pool_state, parent_2));
    BOOST_REQUIRE(!in_pool(pool_state, parent_3));
    BOOST_REQUIRE(pool_state.block_template_bytes == 0u);
    BOOST_REQUIRE(pool_state.block_template_sigops == 0u);

    utilities::sever({ parent_1, parent_2, parent_3, non_anchor });
}

BOOST_AUTO_TEST_CASE(anchor_converter__demote__enqueued_childless_non_anchor_with_mixed_parents__removes_subgraph_returns_node_value)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto non_anchor_1 = utilities::get_entry(state, 1u, 0u);
    auto non_anchor_parent_1 = utilities::get_entry(state, 2u, 0u);
    auto non_anchor_parent_2 = utilities::get_entry(state, 3u, 0u);
    auto parent_1 = utilities::get_entry(state, 4u, 0u);
    auto parent_2 = utilities::get_entry(state, 5u, 0u);
    auto parent_3 = utilities::get_entry(state, 6u, 0u);
    auto parent_4 = utilities::get_entry(state, 7u, 0u);
    auto parent_5 = utilities::get_entry(state, 8u, 0u);

    utilities::connect(non_anchor_parent_1, non_anchor_1, 0u);
    utilities::connect(non_anchor_parent_2, non_anchor_1, 0u);
    utilities::connect(parent_1, non_anchor_1, 0u);
    utilities::connect(parent_2, non_anchor_parent_1, 0u);
    utilities::connect(parent_3, non_anchor_parent_1, 0u);
    utilities::connect(parent_4, non_anchor_parent_2, 0u);
    utilities::connect(parent_5, non_anchor_parent_2, 0u);

    insert_block_template(pool_state, non_anchor_1, 1.0);
    insert_block_template(pool_state, non_anchor_parent_1, 2.0);
    insert_block_template(pool_state, non_anchor_parent_2, 3.0);
    insert_pool(pool_state, parent_1, 4.0);
    insert_pool(pool_state, parent_2, 5.0);
    insert_pool(pool_state, parent_3, 6.0);
    insert_pool(pool_state, parent_4, 7.0);
    insert_pool(pool_state, parent_5, 8.0);

    BOOST_REQUIRE(in_pool(pool_state, non_anchor_1));
    BOOST_REQUIRE(in_pool(pool_state, non_anchor_parent_1));
    BOOST_REQUIRE(in_pool(pool_state, non_anchor_parent_2));
    BOOST_REQUIRE(in_pool(pool_state, parent_1));
    BOOST_REQUIRE(in_pool(pool_state, parent_2));
    BOOST_REQUIRE(in_pool(pool_state, parent_3));
    BOOST_REQUIRE(in_pool(pool_state, parent_4));
    BOOST_REQUIRE(in_pool(pool_state, parent_5));
    BOOST_REQUIRE_EQUAL(pool_state.block_template_bytes,
        non_anchor_1->size() + non_anchor_parent_1->size() +
        non_anchor_parent_2->size());
    BOOST_REQUIRE_EQUAL(pool_state.block_template_sigops,
        non_anchor_1->sigops() + non_anchor_parent_1->sigops() +
        non_anchor_parent_2->sigops());

    converter.enqueue(non_anchor_1);

    auto expected_bytes = non_anchor_parent_1->size() +
        non_anchor_parent_2->size();

    auto expected_sigops = non_anchor_parent_1->sigops() +
        non_anchor_parent_2->sigops();

    auto result = converter.demote();

    BOOST_REQUIRE_EQUAL(1.0, result);
    BOOST_REQUIRE(!in_pool(pool_state, non_anchor_1));
    BOOST_REQUIRE(in_pool(pool_state, non_anchor_parent_1));
    BOOST_REQUIRE(in_pool(pool_state, non_anchor_parent_2));
    BOOST_REQUIRE(!in_pool(pool_state, parent_1));
    BOOST_REQUIRE(in_pool(pool_state, parent_2));
    BOOST_REQUIRE(in_pool(pool_state, parent_3));
    BOOST_REQUIRE(in_pool(pool_state, parent_4));
    BOOST_REQUIRE(in_pool(pool_state, parent_5));
    BOOST_REQUIRE_EQUAL(pool_state.block_template_bytes, expected_bytes);
    BOOST_REQUIRE_EQUAL(pool_state.block_template_sigops, expected_sigops);

    utilities::sever({ non_anchor_1, non_anchor_parent_1, non_anchor_parent_2,
        parent_1, parent_2, parent_3, parent_4, parent_5 });
}

BOOST_AUTO_TEST_CASE(anchor_converter__demote__enqueued_bounded_child_non_anchor_with_anchor_parents__removes_graph_returns_max_among_non_anchors)
{
    transaction_pool_state pool_state;
    anchor_converter converter(pool_state);

    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto non_anchor_1 = utilities::get_entry(state, 1u, 0u);
    auto parent_1 = utilities::get_entry(state, 2u, 0u);
    auto parent_2 = utilities::get_entry(state, 3u, 0u);
    auto child_1_tx = utilities::get_const_tx(7u, 0u);
    auto child_1 = utilities::get_entry(state, 7u, 0u);
    auto child_2_tx = utilities::get_const_tx(8u, 0u);
    auto child_2 = utilities::get_entry(state, 8u, 0u);

    utilities::connect(non_anchor_1, child_1, 0u);
    utilities::connect(non_anchor_1, child_2, 1u);
    utilities::connect(parent_1, non_anchor_1, 0u);
    utilities::connect(parent_2, non_anchor_1, 0u);

    insert_block_template(pool_state, non_anchor_1, 1.0);
    insert_block_template(pool_state, child_1, 9.0);
    insert_pool(pool_state, child_2, 10.0);
    insert_pool(pool_state, parent_1, 4.0);
    insert_pool(pool_state, parent_2, 5.0);

    BOOST_REQUIRE(in_pool(pool_state, non_anchor_1));
    BOOST_REQUIRE(in_pool(pool_state, parent_1));
    BOOST_REQUIRE(in_pool(pool_state, parent_2));
    BOOST_REQUIRE(in_pool(pool_state, child_1));
    BOOST_REQUIRE(in_pool(pool_state, child_2));
    BOOST_REQUIRE_EQUAL(pool_state.block_template_bytes,
        non_anchor_1->size() + child_1->size());
    BOOST_REQUIRE_EQUAL(pool_state.block_template_sigops,
        non_anchor_1->sigops() + child_1->sigops());

    converter.add_bounds(child_1_tx);
    converter.add_bounds(child_2_tx);
    converter.enqueue(non_anchor_1);

    auto result = converter.demote();

    BOOST_REQUIRE_EQUAL(9.0, result);
    BOOST_REQUIRE(!in_pool(pool_state, non_anchor_1));
    BOOST_REQUIRE(!in_pool(pool_state, parent_1));
    BOOST_REQUIRE(!in_pool(pool_state, parent_2));
    BOOST_REQUIRE(!in_pool(pool_state, child_1));
    BOOST_REQUIRE(!in_pool(pool_state, child_2));
    BOOST_REQUIRE_EQUAL(pool_state.block_template_bytes, 0.0);
    BOOST_REQUIRE_EQUAL(pool_state.block_template_sigops, 0.0);

    utilities::sever({ non_anchor_1, parent_1, parent_2, child_1, child_2 });
}

//BOOST_AUTO_TEST_CASE(anchor_converter__demote__bounded_child_non_anchor_with_mixed_parents__removes_subgraph_returns_max_among_non_anchors)
//{
//    BOOST_REQUIRE(true);
//}

//BOOST_AUTO_TEST_CASE(anchor_converter__demote__unbounded_child_non_anchor_with_anchor_parents__removes_subgraph_returns_max_among_non_anchors)
//{
//    BOOST_REQUIRE(true);
//}

//BOOST_AUTO_TEST_CASE(anchor_converter__demote__unbounded_child_non_anchor_with_mixed_parents__removes_subgraph_returns_max_among_non_anchors)
//{
//    BOOST_REQUIRE(true);
//}

BOOST_AUTO_TEST_SUITE_END()
