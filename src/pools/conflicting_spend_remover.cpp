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

#include <bitcoin/blockchain/pools/conflicting_spend_remover.hpp>

namespace libbitcoin {
namespace blockchain {

conflicting_spend_remover::conflicting_spend_remover(
    transaction_pool_state& state)
	: max_removed_(0.0), state_(state)
{
}

bool conflicting_spend_remover::visit(element_type element)
{
    auto& children = element->children().left;

    // add children to list
    for (auto entry = children.begin(); entry != children.end(); ++entry)
        enqueue(entry->second);

    element->remove_children();
    auto parents = element->parents();

    // sever parent connections, enqueue child-less anchor parents
    for (auto& entry : parents)
    {
        entry->remove_child(element);
        if (entry->is_anchor() && entry->children().size() == 0)
            enqueue(entry);
    }

    // remove entry from pool and template
    auto pool_member = state_.pool.left.find(element);
    if (pool_member != state_.pool.left.end())
        state_.pool.left.erase(pool_member);

    auto template_member = state_.block_template.left.find(element);
    if (template_member != state_.block_template.left.end())
    {
        if (max_removed_ < template_member->second)
            max_removed_ = template_member->second;

        state_.block_template_bytes -= template_member->first->size();
        state_.block_template_sigops -= template_member->first->sigops();
        state_.block_template.erase(
            transaction_pool_state::prioritized_transactions::value_type(
            template_member->first, template_member->second));
    }

    return true;
}

conflicting_spend_remover::priority conflicting_spend_remover::deconflict()
{
    max_removed_ = 0.0;
    evaluate();
	return max_removed_;
}

} // namespace blockchain
} // namespace libbitcoin
