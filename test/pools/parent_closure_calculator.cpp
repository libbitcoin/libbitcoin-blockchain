/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
using namespace bc::chain;
using namespace bc::blockchain;
using namespace bc::blockchain::test::pools;

BOOST_AUTO_TEST_SUITE(parent_closure_calculator_tests)

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__nullptr__returns_empty_list)
{
    transaction_pool_state pool_state;
    parent_closure_calculator calculator(pool_state);
    const auto result = calculator.get_closure(nullptr);
    BOOST_REQUIRE(result.empty());
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__anchor_entry__returns_list_with_single_anchor)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    parent_closure_calculator calculator(pool_state);
    auto entry = utilities::get_entry(state, 1, 0);
    const auto result = calculator.get_closure(entry);
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_REQUIRE(result.front() == entry);
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__entry_with_immediate_parents__returns_entry_plus_parent_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    auto parent1_entry = utilities::get_entry(state, 1, 0);
    auto parent2_entry = utilities::get_entry(state, 2, 0);
    auto child_entry = utilities::get_entry(state, 3, 0);
    utilities::connect(parent1_entry, child_entry, 0);
    utilities::connect(parent2_entry, child_entry, 0);
    parent_closure_calculator calculator(pool_state);
    const auto result = calculator.get_closure(child_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 3u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { child_entry, parent1_entry, parent2_entry }));

    // cleanup
    utilities::sever({ parent1_entry, parent2_entry, child_entry });
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__entry_with_multi_child_parent__returns_entry_plus_parent_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    auto parent1_entry = utilities::get_entry(state, 1, 0);
    auto parent2_entry = utilities::get_entry(state, 2, 0);
    auto child1_entry = utilities::get_entry(state, 3, 0);
    auto child2_entry = utilities::get_entry(state, 4, 0);
    utilities::connect(parent1_entry, child1_entry, 0);
    utilities::connect(parent2_entry, child1_entry, 0);
    utilities::connect(parent1_entry, child2_entry, 1);
    parent_closure_calculator calculator(pool_state);
    const auto result = calculator.get_closure(child1_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 3u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { child1_entry, parent1_entry, parent2_entry }));

    // cleanup
    utilities::sever({ parent1_entry, parent2_entry, child1_entry, child2_entry });
}

BOOST_AUTO_TEST_CASE(parent_closure_calculator__get_closure__entry_with_ancestors__returns_entry_plus_ancestor_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    auto alpha = utilities::get_entry(state, 1, 0);
    auto beta = utilities::get_entry(state, 2, 0);
    auto gamma = utilities::get_entry(state, 3, 0);
    auto delta = utilities::get_entry(state, 4, 0);
    auto epsilon = utilities::get_entry(state, 5, 0);
    auto eta = utilities::get_entry(state, 6, 0);
    utilities::connect(alpha, epsilon, 0);
    utilities::connect(beta, epsilon, 0);
    utilities::connect(alpha, eta, 1);
    utilities::connect(gamma, alpha, 0);
    utilities::connect(delta, gamma, 0);
    parent_closure_calculator calculator(pool_state);
    const auto result = calculator.get_closure(epsilon);
    BOOST_REQUIRE_EQUAL(result.size(), 5u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { alpha, beta, gamma, delta, epsilon }));

    // cleanup
    utilities::sever({ alpha, beta, gamma, delta, epsilon, eta });
}

BOOST_AUTO_TEST_SUITE_END()
