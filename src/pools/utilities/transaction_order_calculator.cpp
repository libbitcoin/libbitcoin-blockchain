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
#include <bitcoin/blockchain/pools/utilities/transaction_order_calculator.hpp>

namespace libbitcoin {
namespace blockchain {

transaction_order_calculator::transaction_order_calculator()
    : ordered_()
{
}

bool transaction_order_calculator::visit(element_type element)
{
    bool reenter = false;
    transaction_entry::list required;
    for (auto& parent : element->parents())
        if (!parent->is_anchor() && !has_encountered(parent))
            required.push_back(parent);

    if (required.size() > 0)
    {
        reenter = true;
        enqueue(element);
        for (auto entry : required)
            enqueue(entry);
    }
    else
        ordered_.push_back(element);

    return !reenter;
}

transaction_entry::list transaction_order_calculator::order_transactions()
{
    ordered_.clear();
    evaluate();
    return ordered_;
}

} // namespace blockchain
} // namespace libbitcoin
