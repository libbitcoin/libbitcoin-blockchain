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
#include <bitcoin/blockchain/pools/utilities/parent_closure_calculator.hpp>

namespace libbitcoin {
namespace blockchain {

parent_closure_calculator::parent_closure_calculator(
    transaction_pool_state& )
{
}

bool parent_closure_calculator::visit(transaction_entry::ptr element)
{
    // add all parents
    for (auto parent : element->parents())
        if (!has_encountered(parent))
            enqueue(parent);

    return true;
}

transaction_entry::list parent_closure_calculator::get_closure(
    transaction_entry::ptr tx)
{
    if (tx != nullptr)
        enqueue(tx);

    evaluate();
    transaction_entry::list result;
    for (auto it = begin_encountered(); it != end_encountered(); ++it)
        result.push_back(it->second);

    return result;
}

} // namespace blockchain
} // namespace libbitcoin
