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

BOOST_AUTO_TEST_SUITE(priority_calculator_tests)

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__no_enqueue__returns_zeros)
{
    priority_calculator calculator;
    const auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(result.first, 0u);
    BOOST_REQUIRE_EQUAL(result.second, 0u);
}

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__anchor_entry_enqueue__returns_zeros)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    auto entry = utilities::get_fee_entry(state, 1, 0, 123);
    priority_calculator calculator;
    calculator.enqueue(entry);
    const auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(result.first, 0u);
    BOOST_REQUIRE_EQUAL(result.second, 0u);
}

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__entry_with_immediate_parents__returns_non_anchor_values)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    auto child = utilities::get_fee_entry(state, 1, 0, 123);
    auto parent_1 = utilities::get_fee_entry(state, 2, 0, 321);
    auto parent_2 = utilities::get_fee_entry(state, 3, 0, 222);
    utilities::connect(parent_1, child, 0);
    utilities::connect(parent_2, child, 1);
    priority_calculator calculator;
    calculator.enqueue(child);
    const auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(result.first, 123);
    BOOST_REQUIRE_EQUAL(child->size(), result.second);
    utilities::sever({ parent_1, parent_2, child });
}

BOOST_AUTO_TEST_CASE(priority_calculator__prioritize__entry_with_ancestor_depth__returns_non_anchor_cumulative_values)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, bc::settings() });
    auto child = utilities::get_fee_entry(state, 1, 0, 123);
    auto parent_1 = utilities::get_fee_entry(state, 2, 0, 321);
    auto parent_2 = utilities::get_fee_entry(state, 3, 0, 222);
    auto parent_3 = utilities::get_fee_entry(state, 4, 0, 567);
    auto parent_4 = utilities::get_fee_entry(state, 5, 0, 765);
    auto parent_5 = utilities::get_fee_entry(state, 6, 0, 987);
    auto parent_6 = utilities::get_fee_entry(state, 7, 0, 789);
    utilities::connect(parent_1, child, 0);
    utilities::connect(parent_2, child, 0);
    utilities::connect(parent_4, child, 0);
    utilities::connect(parent_6, child, 0);
    utilities::connect(parent_3, parent_1, 0);
    utilities::connect(parent_5, parent_3, 0);
    priority_calculator calculator;
    calculator.enqueue(child);
    const auto result = calculator.prioritize();
    BOOST_REQUIRE_EQUAL(result.first, calculator.get_cumulative_fees());
    BOOST_REQUIRE_EQUAL(result.second, calculator.get_cumulative_size());
    BOOST_REQUIRE_EQUAL(result.first, child->fees() + parent_1->fees() + parent_3->fees());
    BOOST_REQUIRE_EQUAL(result.second, child->size() + parent_1->size() + parent_3->size());
    utilities::sever({ parent_1, parent_2, parent_3, parent_4, parent_5, parent_6, child });
}

BOOST_AUTO_TEST_SUITE_END()
