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

#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::blockchain;
using namespace bc::machine;

BOOST_AUTO_TEST_SUITE(transaction_entry_tests)

static const auto default_tx_hash = hash_literal("f702453dd03b0f055e5437d76128141803984fb10acb85fc3b2184fae2f3fa78");

static chain_state::data data()
{
    chain_state::data value;
    value.height = 1;
    value.bits = { 0, { 0 } };
    value.version = { 1, { 0 } };
    value.timestamp = { 0, 0, { 0 } };
    return value;
}

static transaction_const_ptr make_tx(uint32_t version, uint32_t locktime)
{
    const auto tx = std::make_shared<const message::transaction>(version,
        locktime, chain::input::list{}, chain::output::list{});
    tx->metadata.state = std::make_shared<chain_state>(
        chain_state{ data(), {}, 0, 0 });
    return tx;
}

static transaction_const_ptr make_tx()
{
    const auto tx = std::make_shared<const message::transaction>();
    tx->metadata.state = std::make_shared<chain_state>(
        chain_state{ data(), {}, 0, 0 });
    return tx;
}

static transaction_entry::ptr make_instance(uint32_t version, uint32_t locktime)
{
    return std::make_shared<transaction_entry>(transaction_entry(make_tx(
        version, locktime)));
}

static transaction_entry::ptr make_instance()
{
    return std::make_shared<transaction_entry>(transaction_entry(make_tx()));
}

// TODO: add populated tx and test property values.

// construct1/tx

BOOST_AUTO_TEST_CASE(transaction_entry__construct1__default_tx__expected_values)
{
    const transaction_entry instance(make_tx());
    BOOST_REQUIRE(instance.is_anchor());
    BOOST_REQUIRE_EQUAL(instance.fees(), 0u);
    BOOST_REQUIRE_EQUAL(instance.forks(), 0u);
    BOOST_REQUIRE_EQUAL(instance.sigops(), 0u);
    BOOST_REQUIRE_EQUAL(instance.size(), 10u);
    BOOST_REQUIRE(instance.hash() == default_tx_hash);
    BOOST_REQUIRE(instance.parents().empty());
    BOOST_REQUIRE(instance.children().empty());
}

// construct2/hash

BOOST_AUTO_TEST_CASE(transaction_entry__construct1__default_block_hash__expected_values)
{
    const transaction_entry instance(make_tx()->hash());
    BOOST_REQUIRE(instance.is_anchor());
    BOOST_REQUIRE_EQUAL(instance.fees(), 0u);
    BOOST_REQUIRE_EQUAL(instance.forks(), 0u);
    BOOST_REQUIRE_EQUAL(instance.sigops(), 0u);
    BOOST_REQUIRE_EQUAL(instance.size(), 0u);
    BOOST_REQUIRE(instance.hash() == default_tx_hash);
    BOOST_REQUIRE(instance.parents().empty());
    BOOST_REQUIRE(instance.children().empty());
}

// is_anchor

BOOST_AUTO_TEST_CASE(transaction_entry__is_anchor__parents__false)
{
    transaction_entry instance(make_tx());
    const auto parent = make_instance();
    instance.add_parent(parent);
    BOOST_REQUIRE(!instance.is_anchor());
}

BOOST_AUTO_TEST_CASE(transaction_entry__is_anchor__children__true)
{
    transaction_entry instance(make_tx());
    const auto child = make_instance();
    instance.add_child(1, child);
    BOOST_REQUIRE(instance.is_anchor());
}

// mark

////BOOST_AUTO_TEST_CASE(transaction_entry__mark__true__expected)
////{
////    transaction_entry instance(make_tx());
////    instance.mark(true);
////    BOOST_REQUIRE(instance.is_marked());
////}
////
////BOOST_AUTO_TEST_CASE(transaction_entry__mark__true_false__expected)
////{
////    transaction_entry instance(make_tx());
////    instance.mark(true);
////    instance.mark(false);
////    BOOST_REQUIRE(!instance.is_marked());
////}

// is_marked

////BOOST_AUTO_TEST_CASE(transaction_entry__mark__default__false)
////{
////    const transaction_entry instance(make_tx());
////    BOOST_REQUIRE(!instance.is_marked());
////}
////
////BOOST_AUTO_TEST_CASE(transaction_entry__is_marked__true__true)
////{
////    transaction_entry instance(make_tx());
////    instance.mark(true);
////    BOOST_REQUIRE(instance.is_marked());
////}

// add_parent

BOOST_AUTO_TEST_CASE(transaction_entry__add_parent__one__expected_parents)
{
    transaction_entry instance(make_tx());
    const auto parent = make_instance();
    instance.add_parent(parent);
    BOOST_REQUIRE_EQUAL(instance.parents().size(), 1u);
    BOOST_REQUIRE_EQUAL(instance.parents().front(), parent);
}

// add_child

BOOST_AUTO_TEST_CASE(transaction_entry__add_child__one__expected_children)
{
    transaction_entry instance(make_tx());
    const auto child = make_instance();
    instance.add_child(1, child);
    BOOST_REQUIRE_EQUAL(instance.children().size(), 1u);
    auto index_result = instance.children().left.find(1);
    BOOST_REQUIRE(index_result != instance.children().left.end());
    BOOST_REQUIRE_EQUAL(index_result->second, child);
    auto retriever = std::make_shared<transaction_entry>(child->hash());
    auto child_result = instance.children().right.find(retriever);
    BOOST_REQUIRE(child_result != instance.children().right.end());
    BOOST_REQUIRE_EQUAL(child_result->first, child);
}

// remove_child

BOOST_AUTO_TEST_CASE(transaction_entry__remove_child_1__not_found__empty)
{
    transaction_entry instance(make_tx());
    uint32_t index = 1;
    instance.remove_child(index);
    BOOST_REQUIRE(instance.children().empty());
}

BOOST_AUTO_TEST_CASE(transaction_entry__remove_child_1__only_found__empty)
{
    transaction_entry instance(make_tx());
    uint32_t index = 1;
    const auto child = make_instance();
    instance.add_child(index, child);
    BOOST_REQUIRE_EQUAL(instance.children().size(), 1u);
    instance.remove_child(index);
    BOOST_REQUIRE(instance.children().empty());
}

BOOST_AUTO_TEST_CASE(transaction_entry__remove_child_1__one_of_two__expected_one_remains)
{
    auto instance = make_instance(0, 0);
    uint32_t index1 = 1;
    const auto child1 = make_instance(1, 0);
    child1->add_parent(instance);
    BOOST_REQUIRE_EQUAL(child1->parents().size(), 1u);
    uint32_t index2 = 2;
    const auto child2 = make_instance(2, 0);
    child2->add_parent(instance);
    BOOST_REQUIRE_EQUAL(child2->parents().size(), 1u);
    instance->add_child(index1, child1);
    instance->add_child(index2, child2);
    BOOST_REQUIRE_EQUAL(instance->children().size(), 2u);
    instance->remove_child(index2);
    BOOST_REQUIRE_EQUAL(child1->parents().size(), 1u);
    BOOST_REQUIRE_EQUAL(child2->parents().size(), 0u);
    BOOST_REQUIRE_EQUAL(instance->children().size(), 1u);
    auto result = instance->children().right.find(child1);
    BOOST_REQUIRE(result != instance->children().right.end());
    BOOST_REQUIRE_EQUAL(result->first, child1);
    BOOST_REQUIRE_EQUAL(result->second, index1);
}

BOOST_AUTO_TEST_CASE(transaction_entry__remove_child_2__not_found__empty)
{
    transaction_entry instance(make_tx());
    const auto child = make_instance();
    instance.remove_child(child);
    BOOST_REQUIRE(instance.children().empty());
}

BOOST_AUTO_TEST_CASE(transaction_entry__remove_child_2__only_found__empty)
{
    transaction_entry instance(make_tx());
    uint32_t index = 1;
    const auto child = make_instance();
    instance.add_child(index, child);
    BOOST_REQUIRE_EQUAL(instance.children().size(), 1u);
    instance.remove_child(child);
    BOOST_REQUIRE(instance.children().empty());
}

BOOST_AUTO_TEST_CASE(transaction_entry__remove_child_2__one_of_two__expected_one_remains)
{
    auto instance = make_instance(3, 0);
    uint32_t index1 = 1;
    auto child1 = make_instance(2, 0);
    instance->add_child(index1, child1);
    child1->add_parent(instance);
    BOOST_REQUIRE_EQUAL(child1->parents().size(), 1u);
    uint32_t index2 = 2;
    auto child2 = make_instance(1, 0);
    instance->add_child(index2, child2);
    child2->add_parent(instance);
    BOOST_REQUIRE_EQUAL(instance->children().size(), 2u);
    BOOST_REQUIRE_EQUAL(child2->parents().size(), 1u);
    instance->remove_child(child2);
    BOOST_REQUIRE_EQUAL(child1->parents().size(), 1u);
    BOOST_REQUIRE_EQUAL(child2->parents().size(), 0u);
    BOOST_REQUIRE_EQUAL(instance->children().size(), 1u);
    BOOST_REQUIRE_EQUAL(instance->children().right.find(child1)->first, child1);
}

BOOST_AUTO_TEST_SUITE_END()
