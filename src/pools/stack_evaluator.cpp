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

#include <map>

#include <bitcoin/blockchain/pools/stack_evaluator.hpp>

namespace libbitcoin {
namespace blockchain {

void stack_evaluator::enqueue(element_type element)
{
    stack_.push_back(element);
}

void stack_evaluator::evaluate()
{
    encountered_.clear();

    while (!stack_.empty())
    {
        auto element = stack_.back();
        stack_.pop_back();

		if (has_encountered(element))
            continue;

		if (visit(element))
			mark_encountered(element);
    }
}

bool stack_evaluator::has_encountered(element_type element) const
{
    return (encountered_.find(element->hash()) != encountered_.end());
}

void stack_evaluator::mark_encountered(element_type element)
{
	encountered_.insert({ element->hash(), element });
}

stack_evaluator::element_type_natural_key_map::const_iterator
    stack_evaluator::begin_encountered() const
{
    return encountered_.begin();
}

stack_evaluator::element_type_natural_key_map::const_iterator
    stack_evaluator::end_encountered() const
{
    return encountered_.end();
}

} // namespace blockchain
} // namespace libbitcoin
