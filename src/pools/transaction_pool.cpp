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
#include <bitcoin/blockchain/pools/transaction_pool.hpp>

#include <cstddef>
#include <deque>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/pools/anchor_converter.hpp>
#include <bitcoin/blockchain/pools/child_closure_calculator.hpp>
#include <bitcoin/blockchain/pools/conflicting_spend_remover.hpp>
#include <bitcoin/blockchain/pools/parent_closure_calculator.hpp>
#include <bitcoin/blockchain/pools/priority_calculator.hpp>
#include <bitcoin/blockchain/pools/transaction_order_calculator.hpp>

namespace libbitcoin {
namespace blockchain {

// Duplicate tx hashes are disallowed in a block and therefore same in pool.
// A transaction hash that exists unspent in the chain is still not acceptable
// even if the original becomes spent in the same block, because the BIP30
// exmaple implementation simply tests all txs in a new block against
// transactions in previous blocks.

transaction_pool::priority anchor_priority = 0.0;

transaction_pool::transaction_pool(const settings&,
    const bc::settings& bitcoin_settings)
  : bitcoin_settings_(bitcoin_settings)
  ////: reject_conflicts_(settings.reject_conflicts),
  ////  minimum_fee_(settings.minimum_fee_satoshis)
{
}

// TODO: implement.
bool transaction_pool::exists(transaction_const_ptr /*tx*/) const
{
    return false;
}

// TODO: implement (performance optimization for tx filtering via store).
void transaction_pool::filter(get_data_ptr /*message*/) const
{
    BITCOIN_ASSERT_MSG(false, "not implemented");
}

void transaction_pool::fetch_template(merkle_block_fetch_handler handler) const
{
    const size_t height = max_size_t;
    const auto block = std::make_shared<message::merkle_block>(
        bitcoin_settings_);
    handler(error::success, block, height);
}

// TODO: implement mempool message payload discovery.
void transaction_pool::fetch_mempool(size_t count_limit, uint64_t minimum_fee,
    inventory_fetch_handler handler) const
{
    const auto empty = std::make_shared<message::inventory>();
    handler(error::success, empty);
}

// TODO: implement block template discovery.
transaction_entry::list transaction_pool::get_template() const
{
    transaction_entry::list result;
    return result;
}

// TODO: implement mempool message payload discovery.
transaction_entry::list transaction_pool::get_mempool() const
{
    transaction_entry::list result;
    return result;
}

void transaction_pool::add_unconfirmed_transactions(
    const transaction_const_ptr_list& unconfirmed_txs)
{
//    tranasaction_entry::ptr max_introduced(null_hash);
    auto max_introduced = state_.pool.left.end();

    // order the transactions to be added preferring parents before children

    // for each entry to be added, check whether an entry already exists
    // if the entry exists
        // if an anchor, add parent anchors, for each ancestor, recalculate priority
    // if the entry is new, add it

    for (const auto& tx: unconfirmed_txs)
    {
        auto unconfirmed_entry = std::make_shared<transaction_entry>(tx);
        uint32_t i = 0;

        // Add/retrieve anchors for each transaction
        for (const auto& input : tx->inputs())
        {
            auto lookup_entry = std::make_shared<transaction_entry>(
                    input.previous_output().hash());

            const auto it = state_.pool.left.find(lookup_entry);

            auto input_entry = (it != state_.pool.left.end()) ?
                it->first : lookup_entry;

            if (input_entry == lookup_entry)
                state_.pool.insert({ input_entry, anchor_priority });

            unconfirmed_entry->add_child(i, input_entry);
            i++;
        }

        // Add unconfirmed transaction
        priority unconfirmed_priority = calculate_priority(unconfirmed_entry);

        state_.pool.insert({ unconfirmed_entry, unconfirmed_priority });

        // track encountered maximum priority
        if (unconfirmed_priority > max_introduced->second)
            max_introduced = state_.pool.left.find(unconfirmed_entry);
    }

    // Using remembered highest priority inserted new transaction,
    // invalidate cached solution below priority and recompute.
    if (unconfirmed_txs.size() > 0)
    {
        auto projected = state_.pool.project_right(max_introduced);
        update_template(projected);
    }
}

void transaction_pool::remove_transactions(transaction_const_ptr_list& txs)
{
    anchor_converter anchorizer(state_);
    conflicting_spend_remover deconflictor(state_);

    // generate map of initial txs
    for (auto& tx : txs)
        anchorizer.add_bounds(tx);

    // compute coverage of inputs being spent
    std::map<hash_digest, std::map<uint32_t, bool>> input_indicies;
    for (auto& tx : txs)
    {
        for (auto& input : tx->inputs())
        {
            auto it = input_indicies.find(input.previous_output().hash());
            if (it == input_indicies.end())
                input_indicies.insert({ input.previous_output().hash(),
                    { { input.previous_output().index(), true } } });
            else
            {
                if (it->second.find(input.previous_output().index()) == it->second.end())
                    it->second.insert({ input.previous_output().index(), true });
            }
        }
    }

    // walk input transactions,
    for (auto& input_it : input_indicies)
    {
        if (anchorizer.within_bounds(input_it.first))
            continue;

        auto key = std::make_shared<transaction_entry>(input_it.first);
        auto member = state_.pool.left.find(key);
        if (member == state_.pool.left.end())
            continue;

        auto children = member->first->children().left;
        auto remove = (children.size() == input_it.second.size());

        for (auto index_it : input_it.second)
        {
            auto index_child = children.find(index_it.first);
            remove &= (index_child != children.end());
            if (index_child != children.end())
            {
                if (anchorizer.within_bounds(index_child->second->hash()))
                    anchorizer.enqueue(index_child->second);
                else
                    deconflictor.enqueue(index_child->second);
            }
        }

        // remove the anchor as all elements depending upon it will
        // either themselves become anchors or will be removed
        if (remove)
        {
            // NOTE: assert is inappropriate, but used to document assumption
            BITCOIN_ASSERT(member->first->parents().size() == 0);
            member->first->remove_children();
            state_.pool.left.erase(member);
        }
    }

    priority max_from_conflicts = deconflictor.deconflict();

    priority max_from_demotion = anchorizer.demote();

//    priority max_from_conflicts = remove_spend_conflicts(
//        unconditional_removal);
//
//    priority max_from_demotion = demote(conditional_removal, removal_bounds);

    priority max_removed = (max_from_conflicts > max_from_demotion) ?
        max_from_conflicts : max_from_demotion;

    // Using remembered highest priority inserted new transaction,
    // invalidate cached solution below priority and recompute.
    if (txs.size() > 0)
    {
        auto inflection = find_inflection(state_.pool, max_removed);
        update_template(inflection);
    }
}

//transaction_pool::priority transaction_pool::remove_spend_conflicts(
//    std::deque<transaction_entry::ptr>& queue)
//{
//    priority max_removed = 0.0;
//    std::map<hash_digest, bool> pending_removal;
//
//    // for each entry,
//    while (!queue.empty())
//    {
//        auto element = queue.back();
//        queue.pop_back();
//
//        // skip repeated elements
//        if (pending_removal.find(element->hash()) == pending_removal.end())
//            continue;
//
//        auto& children = element->children().left;
//        std::list<uint32_t> indicies;
//
//        // add children to list
//        for (auto entry = children.begin(); entry != children.end(); ++entry)
//            queue.push_back(entry->second);
//
//        element->remove_children();
//
//        auto& parents = element->parents();
//
//        // sever parent connections, enqueue child-less anchor parents
//        for (auto& entry : parents)
//        {
//            entry->remove_child(element);
//            if (entry->is_anchor() && entry->children().size() == 0)
//                queue.push_back(entry);
//        }
//
//        // remove entry from pool and template
//        auto pool_member = state_.pool.left.find(element);
//        if (pool_member != state_.pool.left.end())
//            state_.pool.left.erase(pool_member);
//
//        auto template_member = state_.block_template.left.find(element);
//        if (template_member != state_.block_template.left.end())
//        {
//            if (max_removed < template_member->second)
//                max_removed = template_member->second;
//
//            state_.block_template_bytes -= template_member->first->size();
//            state_.block_template_sigops -= template_member->first->sigops();
//            state_.block_template.erase(prioritized_transactions::value_type(
//                template_member->first, template_member->second));
//        }
//
//        // indicate completion with element
//        pending_removal.insert({ element->hash(), remove });
//    }
//
//    return max_removed;
//
//}

//transaction_pool::priority transaction_pool::demote(
//    std::deque<transaction_entry::ptr>& queue,
//    const std::map<hash_digest, bool>& bounds)
//{
//    priority max_removed = 0.0;
//    std::map<hash_digest, bool> pending_removal;
//
//    // for each entry,
//    while (!queue.empty())
//    {
//        auto element = queue.back();
//        queue.pop_back();
//
//        // skip repeated elements
//        if (pending_removal.find(element->hash()) != pending_removal.end())
//            continue;
//
//        auto& children = element->children().left;
//        std::list<uint32_t> indicies;
//        bool remove = true;
//
//        // add children to list
//        for (auto entry = children.begin(); entry != children.end(); ++entry)
//        {
//            if ((bounds.find(entry->second->hash()) != bounds.end()))
//            {
//                indicies.push_back(entry->first);
//                queue.push_back(entry->second);
//            }
//            else
//                remove = false;
//        }
//
//        auto& parents = element->parents();
//
//        // sever parent connections, enqueue child-less anchor parents
//        for (auto& entry : parents)
//        {
//            entry->remove_child(element);
//            if (entry->is_anchor() && entry->children().size() == 0)
//                queue.push_back(entry);
//        }
//
//        // remove children examined from entry
//        for (auto i : indicies)
//            element->remove_child(i);
//
//        if (remove)
//        {
//            // remove entry from pool and template
//            auto pool_member = state_.pool.left.find(element);
//            if (pool_member != state_.pool.left.end())
//                state_.pool.left.erase(pool_member);
//
//            auto template_member = state_.block_template.left.find(element);
//            if (template_member != state_.block_template.left.end())
//            {
//                if (max_removed < template_member->second)
//                    max_removed = template_member->second;
//
//                state_.block_template_bytes -= template_member->first->size();
//                state_.block_template_sigops -= template_member->first->sigops();
//                state_.block_template.erase(prioritized_transactions::value_type(
//                    template_member->first, template_member->second));
//            }
//        }
//
//        // indicate completion with element
//        pending_removal.insert({ element->hash(), remove });
//    }
//
//    return max_removed;
//}

transaction_pool::priority transaction_pool::calculate_priority(
    transaction_entry::ptr tx)
{
    priority_calculator calculator;
    calculator.enqueue(tx);
    auto result = calculator.prioritize();
    uint64_t cumulative_fees = result.first;
    size_t cumulative_size = result.second;

    // return ratio of size to rate
    return (cumulative_size > 0) ? cumulative_fees / cumulative_size :
        std::numeric_limits<transaction_pool::priority>::max();
}

transaction_pool::priority_iterator transaction_pool::find_inflection(
    transaction_pool_state::prioritized_transactions& container,
    transaction_pool::priority value)
{
    priority_iterator changepoint = container.right.begin();

    for (auto it = container.right.begin(); it != container.right.end(); ++it)
    {
        if (it->first <= value)
        {
            changepoint = it;
            break;
        }
    }

    return changepoint;
}

struct plus_sigops
{
    size_t operator()(size_t lhs, const transaction_entry::ptr& rhs) const
    {
        return rhs ? lhs + rhs->sigops() : lhs;
    }
};

struct plus_size
{
    size_t operator()(size_t lhs, const transaction_entry::ptr& rhs) const
    {
        return rhs ? lhs + rhs->size() : lhs;
    }
};

void transaction_pool::update_template(priority_iterator max_pool_change)
{
    // as max_changepoint may not be a value within the template,
    // walk the template entries until changepoint or a value less than it is
    // discovered
    auto template_point = find_inflection(state_.block_template,
        max_pool_change->first);

    // for each element in the template below this point, purge if
    // not depended upon by an entry of higher priority
    // alternately: scan closure of children for references in the template
    // above this prioritization value
    transaction_entry::list to_remove;
    for (auto entry = template_point; entry != state_.block_template.right.end(); ++entry)
    {
        bool purge = true;
        auto closure_element = state_.cached_child_closures.find(entry->second);

        if (closure_element != state_.cached_child_closures.end())
        {
            for (auto child : closure_element->second)
            {
                auto child_in_template = state_.block_template.left.find(child);
                if ((child_in_template != state_.block_template.left.end()) &&
                    (child_in_template->second > max_pool_change->first))
                {
                    purge = false;
                    break;
                }
            }
        }

        if (purge)
            to_remove.push_back(closure_element->first);
    }

    auto pool_point = max_pool_change;

    // for each element after the inflection point,
    for (auto it = pool_point; it != state_.pool.right.end(); ++it)
    {
        // if within template, skip
        if (state_.block_template.left.find(it->second) != state_.block_template.left.end())
            continue;

        // if outside template, test addition against sigops/size limits
        auto proposed_entries = get_parent_closure(it->second);
        size_t cumulative_sigops = std::accumulate(proposed_entries.begin(),
            proposed_entries.end(), 0, plus_sigops());
        size_t cumulative_bytes = std::accumulate(proposed_entries.begin(),
            proposed_entries.end(), 0, plus_size());

        if ((cumulative_sigops + state_.block_template_sigops +
            state_.coinbase_sigop_reserve <= state_.template_sigop_limit) &&
            (cumulative_bytes + state_.block_template_bytes +
            state_.coinbase_byte_reserve <= state_.template_byte_limit))
        {
            state_.block_template_bytes += cumulative_bytes;
            state_.block_template_sigops += cumulative_sigops;

            for (auto entry : proposed_entries)
            {
                auto pool_entry = state_.pool.left.find(entry);
                state_.block_template.insert({ pool_entry->first,
                    pool_entry->second });

                // calculate closure over children for fast filtering on
                // subsequent update
                populate_child_closure(pool_entry->first);
            }
        }
    }

    // Regenerate ordered template
    order_template_transactions();

    // NOTE: detection of change requires memory of purged and added sets
    // for difference computation.  Not currently implemented.
}

void transaction_pool::order_template_transactions()
{
    transaction_order_calculator calculator;
    for (auto entry : state_.block_template.left)
        calculator.enqueue(entry.first);

    state_.ordered_block_template = calculator.order_transactions();
}

void transaction_pool::populate_child_closure(transaction_entry::ptr tx)
{
    child_closure_calculator calculator(state_);
    state_.cached_child_closures.insert({ tx, calculator.get_closure(tx) });
}

transaction_entry::list transaction_pool::get_parent_closure(
    transaction_entry::ptr tx)
{
    parent_closure_calculator calculator(state_);
    return calculator.get_closure(tx);
}

} // namespace blockchain
} // namespace libbitcoin
