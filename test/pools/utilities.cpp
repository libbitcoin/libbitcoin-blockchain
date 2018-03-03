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

#include "utilities.hpp"

namespace libbitcoin {
namespace blockchain {
namespace test {
namespace pools {

using namespace bc;
using namespace bc::chain;
using namespace bc::blockchain;

chain_state::data utilities::get_chain_data()
{
    chain_state::data value;
    value.height = 1;
    value.bits = { 0, { 0 } };
    value.version = { 1, { 0 } };
    value.timestamp = { 0, 0, { 0 } };
    return value;
}

transaction_const_ptr utilities::get_const_tx(uint32_t version,
    uint32_t locktime)
{
    return std::make_shared<const message::transaction>(
        message::transaction{ version, locktime, {}, {} });
}

transaction_entry::ptr utilities::get_entry(chain_state::ptr state,
    uint32_t version, uint32_t locktime)
{
    auto tx = get_const_tx(version, locktime);
    tx->metadata.state = state;
    return std::make_shared<transaction_entry>(tx);
}

transaction_entry::ptr utilities::get_fee_entry(chain_state::ptr state,
    uint32_t version, uint32_t locktime, uint64_t fee)
{
    message::transaction tx(version, locktime, {}, {});
    input in;
    output_point point;
    point.metadata.cache.set_value(fee);
    in.set_previous_output(point);
    input::list ins{ in };
    tx.set_inputs(ins);
    auto tx_ptr = std::make_shared<const message::transaction>(tx);
    tx_ptr->metadata.state = state;
    return std::make_shared<transaction_entry>(tx_ptr);
}

void utilities::connect(transaction_entry::ptr parent,
    transaction_entry::ptr child, uint32_t index)
{
    parent->add_child(index, child);
    child->add_parent(parent);
}

void utilities::sever(transaction_entry::ptr entry)
{
    entry->remove_children();
    entry->remove_parents();
}

void utilities::sever(transaction_entry::list entries)
{
    for (auto entry: entries)
        sever(entry);
}

bool utilities::ordered_entries_equal(transaction_entry::list left,
    transaction_entry::list right)
{
    auto equal = (left.size() == right.size());
    auto left_it = left.begin();
    auto right_it = right.begin();

    while (equal && (left_it != left.end()) && (right_it != right.end()))
    {
        if (*left_it != *right_it)
            equal = false;

        ++left_it;
        ++right_it;
    }

    if (equal)
        equal = (left_it == left.end()) && (right_it == right.end());

    return equal;
}

bool utilities::unordered_entries_equal(transaction_entry::list left,
    transaction_entry::list right)
{
    auto equal = (left.size() == right.size());
    std::map<uint32_t, bool> encountered;

    if (equal)
    {
        for (auto left_value : left)
        {
            auto found = false;
            uint32_t counter = 0;

            for (auto right_value : right)
            {
                if (left_value == right_value)
                {
                    encountered.insert({ counter, true });
                    found = true;
                    break;
                }

                ++counter;
            }

            if (!found)
            {
                equal = false;
                break;
            }
        }
    }

    if (equal)
        equal = (encountered.size() == left.size());

    return equal;
}

} // namespace pools
} // namespace test
} // namespace blockchain
} // namespace libbitcoin
