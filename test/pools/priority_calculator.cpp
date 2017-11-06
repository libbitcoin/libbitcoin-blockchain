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

BOOST_AUTO_TEST_SUITE(priority_calculator_tests)

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__no_enqueue__returns_zeros)
{
    priority_calculator calculator;
    auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(0u, result.first);
    BOOST_REQUIRE_EQUAL(0u, result.second);
}

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__anchor_entry_enqueue__returns_zeros)
{
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto entry = utilities::get_fee_entry(state, 1u, 0u, 123u);

    priority_calculator calculator;
    calculator.enqueue(entry);
    auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(0u, result.first);
    BOOST_REQUIRE_EQUAL(0u, result.second);
}

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__entry_with_immediate_parents__returns_non_anchor_values)
{
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto child = utilities::get_fee_entry(state, 1u, 0u, 123u);
    auto parent_1 = utilities::get_fee_entry(state, 2u, 0u, 321u);
    auto parent_2 = utilities::get_fee_entry(state, 3u, 0u, 222u);
    utilities::connect(parent_1, child, 0u);
    utilities::connect(parent_2, child, 1u);

    priority_calculator calculator;
    calculator.enqueue(child);
    auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(123u, result.first);
    BOOST_REQUIRE_EQUAL(child->size(), result.second);

    utilities::sever({ parent_1, parent_2, child });
}

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__entry_with_ancestor_depth__returns_non_anchor_cumulative_values)
{
    chain::chain_state::ptr state = std::make_shared<chain::chain_state>(
        chain::chain_state{ utilities::get_chain_data(), {}, 0u });

    auto child = utilities::get_fee_entry(state, 1u, 0u, 123u);
    auto parent_1 = utilities::get_fee_entry(state, 2u, 0u, 321u);
    auto parent_2 = utilities::get_fee_entry(state, 3u, 0u, 222u);
    auto parent_3 = utilities::get_fee_entry(state, 4u, 0u, 567u);
    auto parent_4 = utilities::get_fee_entry(state, 5u, 0u, 765u);
    auto parent_5 = utilities::get_fee_entry(state, 6u, 0u, 987u);
    auto parent_6 = utilities::get_fee_entry(state, 7u, 0u, 789u);
    utilities::connect(parent_1, child, 0u);
    utilities::connect(parent_2, child, 0u);
    utilities::connect(parent_4, child, 0u);
    utilities::connect(parent_6, child, 0u);
    utilities::connect(parent_3, parent_1, 0u);
    utilities::connect(parent_5, parent_3, 0u);

    priority_calculator calculator;
    calculator.enqueue(child);
    auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(result.first,
        child->fees() + parent_1->fees() + parent_3->fees());

    BOOST_REQUIRE_EQUAL(result.second,
        child->size() + parent_1->size() + parent_3->size());

    utilities::sever({ parent_1, parent_2, parent_3, parent_4, parent_5,
        parent_6, child });
}

BOOST_AUTO_TEST_SUITE_END()
