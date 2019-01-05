/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(conflicting_spend_remover_tests)

BOOST_AUTO_TEST_CASE(conflicting_spend_remover__deconflict__empty__returns_zero)
{
    transaction_pool_state pool_state;
    conflicting_spend_remover remover(pool_state);
    const auto result = remover.deconflict();
    BOOST_REQUIRE_EQUAL(result, 0.0);
}

BOOST_AUTO_TEST_CASE(conflicting_spend_remover__deconflict__childless_entry_outside_template__returns_zero)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    transaction_pool_state pool_state;
    conflicting_spend_remover remover(pool_state);
    auto entry = utilities::get_entry(state, 1, 0u);
    insert_pool(pool_state, entry, 0.5);
    remover.enqueue(entry);
    const auto result = remover.deconflict();
    BOOST_REQUIRE_EQUAL(result, 0.0);
    utilities::sever(entry);
}

BOOST_AUTO_TEST_CASE(conflicting_spend_remover__deconflict__childless_entry_within_template__returns_entry_priority)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    transaction_pool_state pool_state;
    conflicting_spend_remover remover(pool_state);
    auto entry = utilities::get_entry(state, 1, 0);
    insert_block_template(pool_state, entry, 0.5);
    remover.enqueue(entry);
    auto result = remover.deconflict();
    BOOST_REQUIRE_EQUAL(result, 0.5);
    utilities::sever(entry);
}

BOOST_AUTO_TEST_CASE(conflicting_spend_remover__deconflict__entry_with_multi_parent_child__returns_max_priority_in_decendant_graph)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    transaction_pool_state pool_state;
    conflicting_spend_remover remover(pool_state);
    auto parent_1 = utilities::get_entry(state, 1, 0);
    auto parent_2 = utilities::get_entry(state, 2, 0);
    auto parent_3 = utilities::get_entry(state, 3, 0);
    auto child = utilities::get_entry(state, 4, 0u);
    utilities::connect(parent_1, child, 0);
    utilities::connect(parent_2, child, 0);
    utilities::connect(parent_3, child, 0);
    insert_block_template(pool_state, parent_1, 0.5);
    insert_block_template(pool_state, child, 0.75);
    remover.enqueue(parent_1);
    const auto result = remover.deconflict();
    BOOST_REQUIRE_EQUAL(result, 0.75);
    utilities::sever({ parent_1, parent_2, parent_3, child });
}

BOOST_AUTO_TEST_CASE(conflicting_spend_remover__deconflict__entry_with_immediate_children__returns_max_priority_in_decendant_graph)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    transaction_pool_state pool_state;
    conflicting_spend_remover remover(pool_state);
    auto parent = utilities::get_entry(state, 1, 0);
    auto child_1 = utilities::get_entry(state, 2, 0);
    auto child_2 = utilities::get_entry(state, 3, 0);
    auto child_3 = utilities::get_entry(state, 4, 0);
    auto child_4 = utilities::get_entry(state, 5, 0);
    utilities::connect(parent, child_1, 0);
    utilities::connect(parent, child_2, 1);
    utilities::connect(parent, child_3, 2);
    utilities::connect(parent, child_4, 3);
    insert_block_template(pool_state, child_1, 0.2);
    insert_block_template(pool_state, child_2, 0.4);
    insert_block_template(pool_state, child_3, 0.6);
    insert_block_template(pool_state, child_4, 0.3);

    remover.enqueue(parent);
    const auto result = remover.deconflict();
    BOOST_REQUIRE_EQUAL(result, 0.6);
    utilities::sever({ parent, child_1, child_2, child_3, child_4 });
}

BOOST_AUTO_TEST_CASE(conflicting_spend_remover__deconflict__entry_with_descendants__returns_max_priority_in_decendant_graph)
{
    auto state = std::make_shared<chain_state>(
        chain_state{ utilities::get_chain_data(), {}, 0, 0, system::settings() });
    transaction_pool_state pool_state;
    conflicting_spend_remover remover(pool_state);
    auto parent = utilities::get_entry(state, 1, 0);
    auto child_1 = utilities::get_entry(state, 2, 0);
    auto child_2 = utilities::get_entry(state, 3, 0);
    auto child_3 = utilities::get_entry(state, 4, 0);
    auto child_4 = utilities::get_entry(state, 5, 0);
    utilities::connect(parent, child_1, 0);
    utilities::connect(child_1, child_2, 0);
    utilities::connect(child_2, child_3, 0);
    utilities::connect(child_2, child_4, 1);
    insert_block_template(pool_state, child_1, 0.2);
    insert_block_template(pool_state, child_2, 0.4);
    insert_block_template(pool_state, child_3, 0.6);
    insert_block_template(pool_state, child_4, 0.3);
    remover.enqueue(parent);
    const auto result = remover.deconflict();
    BOOST_REQUIRE_EQUAL(result, 0.6);
    utilities::sever({ parent, child_1, child_2, child_3, child_4 });

}

BOOST_AUTO_TEST_SUITE_END()
