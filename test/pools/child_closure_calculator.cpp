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

BOOST_AUTO_TEST_SUITE(child_closure_calculator_tests)

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__nullptr__returns_empty_list)
{
    transaction_pool_state pool_state;
    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(nullptr);
    BOOST_REQUIRE_EQUAL(0u, result.size());
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__childless_entry__returns_empty_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    child_closure_calculator calculator(pool_state);
    transaction_entry::ptr entry = utilities::get_entry(state, 1u, 0u);
    auto result = calculator.get_closure(entry);
    BOOST_REQUIRE_EQUAL(0u, result.size());
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_immediate_child__returns_child_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr parent_entry = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr child1_entry = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr child2_entry = utilities::get_entry(state, 3u, 0u);
    utilities::connect(parent_entry, child1_entry, 0u);
    utilities::connect(parent_entry, child2_entry, 1u);

    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(2u, result.size());
    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { child1_entry, child2_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_multi_parent_child__returns_child_list)
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
    utilities::connect(parent2_entry, child2_entry, 1u);

    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent1_entry);
    BOOST_REQUIRE_EQUAL(1u, result.size());
    BOOST_REQUIRE(child1_entry == result.front());

    // cleanup
    utilities::sever({ parent1_entry, parent2_entry, child1_entry, child2_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_immediate_children__returns_children_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr parent_entry = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr child1_entry = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr child2_entry = utilities::get_entry(state, 3u, 0u);
    transaction_entry::ptr child3_entry = utilities::get_entry(state, 4u, 0u);
    utilities::connect(parent_entry, child1_entry, 0u);
    utilities::connect(parent_entry, child2_entry, 1u);
    utilities::connect(parent_entry, child3_entry, 2u);

    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(3u, result.size());

    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { child1_entry, child2_entry, child3_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry, child3_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_descendants__returns_descendant_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr parent_entry = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr child1_entry = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr child2_entry = utilities::get_entry(state, 3u, 0u);
    transaction_entry::ptr child3_entry = utilities::get_entry(state, 4u, 0u);
    utilities::connect(parent_entry, child1_entry, 0u);
    utilities::connect(child1_entry, child2_entry, 0u);
    utilities::connect(child1_entry, child3_entry, 1u);
    utilities::connect(child2_entry, child3_entry, 0u);

    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(3u, result.size());

    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { child1_entry, child2_entry, child3_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry, child3_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_descendants_state_cached_child_closure__returns_descendant_list)
{
    transaction_pool_state pool_state;
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    transaction_entry::ptr parent_entry = utilities::get_entry(state, 1u, 0u);
    transaction_entry::ptr child1_entry = utilities::get_entry(state, 2u, 0u);
    transaction_entry::ptr child2_entry = utilities::get_entry(state, 3u, 0u);
    transaction_entry::ptr child3_entry = utilities::get_entry(state, 4u, 0u);
    utilities::connect(parent_entry, child1_entry, 0u);
    utilities::connect(child1_entry, child2_entry, 0u);
    utilities::connect(child2_entry, child3_entry, 0u);

    // populate cache
    pool_state.cached_child_closures.insert({ child1_entry,
        { child2_entry, child3_entry } });

    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(3u, result.size());

    BOOST_REQUIRE(utilities::unordered_entries_equal(result,
        { child1_entry, child2_entry, child3_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry, child3_entry });
}

BOOST_AUTO_TEST_SUITE_END()
