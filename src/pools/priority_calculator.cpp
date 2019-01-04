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

#include <utility>
#include <bitcoin/blockchain/pools/priority_calculator.hpp>

namespace libbitcoin {
namespace blockchain {

priority_calculator::priority_calculator()
    : cumulative_fees_(0), cumulative_size_(0)
{
}

bool priority_calculator::visit(transaction_entry::ptr element)
{
    // add all parents
    for (auto parent : element->parents())
        enqueue(parent);

    // increment cumulative sums
    if (!element->is_anchor())
    {
        cumulative_fees_ += element->fees();
        cumulative_size_ += element->size();
    }

    return true;
}

uint64_t priority_calculator::get_cumulative_fees() const
{
    return cumulative_fees_;
}

size_t priority_calculator::get_cumulative_size() const
{
    return cumulative_size_;
}

std::pair<uint64_t, size_t> priority_calculator::prioritize()
{
    cumulative_fees_ = 0;
    cumulative_size_ = 0;
    evaluate();
    return std::make_pair(cumulative_fees_, cumulative_size_);
}

} // namespace blockchain
} // namespace libbitcoin
