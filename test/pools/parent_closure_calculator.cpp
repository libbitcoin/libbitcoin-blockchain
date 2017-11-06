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

BOOST_AUTO_TEST_SUITE(parent_closure_calculator_tests)

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__nullptr__returns_empty_list)
{
    transaction_pool_state pool_state;
    parent_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(nullptr);
    BOOST_REQUIRE_EQUAL(0u, result.size());
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__anchor_entry__returns_list_with_single_anchor)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    parent_closure_calculator calculator(pool_state);
    transaction_entry::ptr entry = utilities::get_entry(state, 1u, 0u);
    auto result = calculator.get_closure(entry);
    BOOST_REQUIRE_EQUAL(1u, result.size());
    BOOST_REQUIRE(entry == result.front());
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__entry_with_immediate_parents__returns_entry_plus_parent_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr parent1_entry = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr parent2_entry = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr child_entry = utilities::get_entry(state, 3u, 0u);
    utilities::connect(parent1_entry, child_entry, 0u);
    utilities::connect(parent2_entry, child_entry, 0u);

    parent_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(child_entry);
    BOOST_REQUIRE_EQUAL(3u, result.size());
    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { child_entry, parent1_entry, parent2_entry }));

    // cleanup
    utilities::sever({ parent1_entry, parent2_entry, child_entry });
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__entry_with_multi_child_parent__returns_entry_plus_parent_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr parent1_entry = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr parent2_entry = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr child1_entry = utilities::get_entry(state, 3u, 0u);
    transaction_entry::ptr child2_entry = utilities::get_entry(state, 4u, 0u);
    utilities::connect(parent1_entry, child1_entry, 0u);
    utilities::connect(parent2_entry, child1_entry, 0u);
    utilities::connect(parent1_entry, child2_entry, 1u);

    parent_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(child1_entry);
    BOOST_REQUIRE_EQUAL(3u, result.size());
    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { child1_entry, parent1_entry, parent2_entry }));

    // cleanup
    utilities::sever({ parent1_entry, parent2_entry, child1_entry, child2_entry });
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__entry_with_ancestors__returns_entry_plus_ancestor_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr alpha = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr beta = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr gamma = utilities::get_entry(state, 3u, 0u);
    transaction_entry::ptr delta = utilities::get_entry(state, 4u, 0u);
    transaction_entry::ptr epsilon = utilities::get_entry(state, 5u, 0u);
    transaction_entry::ptr eta = utilities::get_entry(state, 6u, 0u);
    utilities::connect(alpha, epsilon, 0u);
    utilities::connect(beta, epsilon, 0u);
    utilities::connect(alpha, eta, 1u);
    utilities::connect(gamma, alpha, 0u);
    utilities::connect(delta, gamma, 0u);

    parent_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(epsilon);
    BOOST_REQUIRE_EQUAL(5u, result.size());
    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { alpha, beta, gamma, delta, epsilon }));

    // cleanup
    utilities::sever({ alpha, beta, gamma, delta, epsilon, eta });
}

BOOST_AUTO_TEST_SUITE_END()
