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
using namespace bc::blockchain;
using namespace bc::blockchain::test::pools;
using namespace bc::system;
using namespace bc::system::chain;

BOOST_AUTO_TEST_SUITE(child_closure_calculator_tests)

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__nullptr__returns_empty_list)
{
    transaction_pool_state pool_state;
    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(nullptr);
    BOOST_REQUIRE(result.empty());
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__childless_entry__returns_empty_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    child_closure_calculator calculator(pool_state);
    auto entry = utilities::get_entry(state, 1, 0);
    auto result = calculator.get_closure(entry);
    BOOST_REQUIRE(result.empty());
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_immediate_child__returns_child_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain::chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    auto parent_entry = utilities::get_entry(state, 1, 0);
    auto child1_entry = utilities::get_entry(state, 2, 0);
    auto child2_entry = utilities::get_entry(state, 3, 0);
    utilities::connect(parent_entry, child1_entry, 0);
    utilities::connect(parent_entry, child2_entry, 1);
    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { child1_entry, child2_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_multi_parent_child__returns_child_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    auto parent1_entry = utilities::get_entry(state, 1, 0);
    auto parent2_entry = utilities::get_entry(state, 2, 0);
    auto child1_entry = utilities::get_entry(state, 3, 0);
    auto child2_entry = utilities::get_entry(state, 4, 0);
    utilities::connect(parent1_entry, child1_entry, 0);
    utilities::connect(parent2_entry, child1_entry, 0);
    utilities::connect(parent2_entry, child2_entry, 1);
    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent1_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_REQUIRE(child1_entry == result.front());

    // cleanup
    utilities::sever({ parent1_entry, parent2_entry, child1_entry, child2_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_immediate_children__returns_children_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain::chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    auto parent_entry = utilities::get_entry(state, 1, 0);
    auto child1_entry = utilities::get_entry(state, 2, 0);
    auto child2_entry = utilities::get_entry(state, 3, 0);
    auto child3_entry = utilities::get_entry(state, 4, 0);
    utilities::connect(parent_entry, child1_entry, 0);
    utilities::connect(parent_entry, child2_entry, 1);
    utilities::connect(parent_entry, child3_entry, 2);
    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 3u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { child1_entry, child2_entry, child3_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry, child3_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_descendants__returns_descendant_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain::chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    auto parent_entry = utilities::get_entry(state, 1, 0);
    auto child1_entry = utilities::get_entry(state, 2, 0);
    auto child2_entry = utilities::get_entry(state, 3, 0);
    auto child3_entry = utilities::get_entry(state, 4, 0);
    utilities::connect(parent_entry, child1_entry, 0);
    utilities::connect(child1_entry, child2_entry, 0);
    utilities::connect(child1_entry, child3_entry, 1);
    utilities::connect(child2_entry, child3_entry, 0);
    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 3u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { child1_entry, child2_entry, child3_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry, child3_entry });
}

BOOST_AUTO_TEST_CASE(child_closure_calculator__get_closure__entry_with_descendants_state_cached_child_closure__returns_descendant_list)
{
    transaction_pool_state pool_state;
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    auto parent_entry = utilities::get_entry(state, 1, 0);
    auto child1_entry = utilities::get_entry(state, 2, 0);
    auto child2_entry = utilities::get_entry(state, 3, 0);
    auto child3_entry = utilities::get_entry(state, 4, 0);
    utilities::connect(parent_entry, child1_entry, 0);
    utilities::connect(child1_entry, child2_entry, 0);
    utilities::connect(child2_entry, child3_entry, 0);

    // populate cache
    pool_state.cached_child_closures.insert({ child1_entry, { child2_entry, child3_entry } });

    child_closure_calculator calculator(pool_state);
    auto result = calculator.get_closure(parent_entry);
    BOOST_REQUIRE_EQUAL(result.size(), 3u);
    BOOST_REQUIRE(utilities::unordered_entries_equal(result, { child1_entry, child2_entry, child3_entry }));

    // cleanup
    utilities::sever({ parent_entry, child1_entry, child2_entry, child3_entry });
}

BOOST_AUTO_TEST_SUITE_END()
