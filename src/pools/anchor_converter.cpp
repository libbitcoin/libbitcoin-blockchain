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

#include <bitcoin/blockchain/pools/anchor_converter.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

anchor_converter::anchor_converter(transaction_pool_state& state)
    : bounds_(), max_removed_(0.0), state_(state)
{
}

bool anchor_converter::visit(element_type element)
{
    auto& children = element->children().left;
    std::list<uint32_t> indicies;
    bool remove = true;

    // add children to list
    for (auto entry = children.begin(); entry != children.end(); ++entry)
    {
        if ((bounds_.find(entry->second->hash()) != bounds_.end()))
        {
            indicies.push_back(entry->first);
            enqueue(entry->second);
        }
        else
            remove = false;
    }

    auto parents = element->parents();

    // sever parent connections, enqueue child-less anchor parents
    for (auto& entry : parents)
    {
        entry->remove_child(element);
        if (entry->is_anchor() && entry->children().size() == 0)
            enqueue(entry);
    }

    // remove children examined from entry
    for (auto i : indicies)
        element->remove_child(i);

    // remove entry from template if present
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

    // remove entry from pool if no child will remain
    if (remove)
    {
        auto pool_member = state_.pool.left.find(element);
        if (pool_member != state_.pool.left.end())
            state_.pool.left.erase(pool_member);
    }

    return true;
}

anchor_converter::priority anchor_converter::demote()
{
    max_removed_ = 0.0;
    evaluate();
    return max_removed_;
}

void anchor_converter::add_bounds(transaction_const_ptr tx)
{
    bounds_.insert({ tx->hash(), true });
}

bool anchor_converter::within_bounds(hash_digest digest)
{
    return (bounds_.find(digest) != bounds_.end());
}

} // namespace blockchain
} // namespace libbitcoin
