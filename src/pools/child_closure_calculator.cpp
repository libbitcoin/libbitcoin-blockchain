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

#include <bitcoin/blockchain/pools/child_closure_calculator.hpp>

namespace libbitcoin {
namespace blockchain {

child_closure_calculator::child_closure_calculator(
    transaction_pool_state& state)
    : state_(state)
{
}

bool child_closure_calculator::visit(transaction_entry::ptr element)
{
    auto closure_point = state_.cached_child_closures.find(element);
    if (closure_point != state_.cached_child_closures.end())
    {
        // short circuit exploration by using the existing closure for
        // this child node
        for (auto it : closure_point->second)
        {
            if (!has_encountered(it))
            {
                mark_encountered(it);
                closure_.push_back(it);
            }
        }
    }
    else
    {
        // enqueue children
        auto& child_left = element->children().left;
        for (auto it = child_left.begin(); it != child_left.end(); ++it)
            enqueue(it->second);
    }

    closure_.push_back(element);

    return true;
}

transaction_entry::list child_closure_calculator::get_closure(transaction_entry::ptr tx)
{
    closure_.clear();

    if (tx != nullptr)
    {
        auto children = tx->children();
        for (auto it = children.left.begin(); it != children.left.end(); ++it)
            enqueue(it->second);
    }

    evaluate();
    return closure_;
}

} // namespace blockchain
} // namespace libbitcoin
