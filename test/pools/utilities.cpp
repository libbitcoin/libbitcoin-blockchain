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

bc::chain::chain_state::data utilities::get_chain_data()
{
    bc::chain::chain_state::data value;
    value.height = 1;
    value.bits = { 0, { 0 } };
    value.version = { 1, { 0 } };
    value.timestamp = { 0, 0, { 0 } };
    return value;
}

bc::transaction_const_ptr utilities::get_const_tx(uint32_t version,
    uint32_t locktime)
{
    bc::transaction_const_ptr tx = std::make_shared<
        const bc::message::transaction>(bc::message::transaction{ version, locktime, {}, {} });

    return tx;
}

bc::blockchain::transaction_entry::ptr utilities::get_entry(
    bc::chain::chain_state::ptr state, uint32_t version, uint32_t locktime)
{
    bc::transaction_const_ptr tx = get_const_tx(version, locktime);

    tx->validation.state = state;
    bc::blockchain::transaction_entry::ptr entry = std::make_shared<
            bc::blockchain::transaction_entry>(tx);

    return entry;
}

bc::blockchain::transaction_entry::ptr utilities::get_fee_entry(
    bc::chain::chain_state::ptr state, uint32_t version,
    uint32_t locktime, uint64_t fee)
{
    bc::message::transaction tx(version, locktime, {}, {});
    bc::chain::input in;
    bc::chain::output_point point;
    point.validation.cache.set_value(fee);
    in.set_previous_output(point);
    bc::chain::input::list ins = { in };
    tx.set_inputs(ins);

    bc::transaction_const_ptr tx_ptr = std::make_shared<
        const bc::message::transaction>(tx);

    tx_ptr->validation.state = state;
    bc::blockchain::transaction_entry::ptr entry = std::make_shared<
            bc::blockchain::transaction_entry>(tx_ptr);

    return entry;
}

void utilities::connect(bc::blockchain::transaction_entry::ptr parent,
    bc::blockchain::transaction_entry::ptr child, uint32_t index)
{
    parent->add_child(index, child);
    child->add_parent(parent);
}

void utilities::sever(bc::blockchain::transaction_entry::ptr entry)
{
    entry->remove_children();
    entry->remove_parents();
}

void utilities::sever(bc::blockchain::transaction_entry::list entries)
{
    for (auto entry : entries)
        sever(entry);
}

bool utilities::ordered_entries_equal(
    bc::blockchain::transaction_entry::list alpha,
    bc::blockchain::transaction_entry::list beta)
{
    bool result = (alpha.size() == beta.size());
    auto a_iter = alpha.begin();
    auto b_iter = beta.begin();

    while (result && (a_iter != alpha.end()) && (b_iter != beta.end()))
    {
        if (*a_iter != *b_iter)
            result = false;

        a_iter++;
        b_iter++;
    }

    if (result)
        result = (a_iter == alpha.end()) && (b_iter == beta.end());

    return result;
}

bool utilities::unordered_entries_equal(
    bc::blockchain::transaction_entry::list alpha,
    bc::blockchain::transaction_entry::list beta)
{
    bool result = (alpha.size() == beta.size());
    std::map<uint32_t, bool> encountered;

    if (result)
    {
        for (auto ith : alpha)
        {
            bool found = false;
            uint32_t j = 0;

            for (auto jth : beta)
            {
                if (ith == jth)
                {
                    encountered.insert({ j, true });
                    found = true;
                    break;
                }

                j++;
            }

            if (!found)
            {
                result = false;
                break;
            }
        }
    }

    if (result)
        result = (encountered.size() == alpha.size());

    return result;
}

} // namespace pools
} // namespace test
} // namespace blockchain
} // namespace libbitcoin
