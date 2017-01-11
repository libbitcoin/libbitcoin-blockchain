/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/pools/block_pool.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>

// Atomicity is not required for these operations as each validation call is
// sequenced. Locking is performed only to guard concurrent filtering.

namespace libbitcoin {
namespace blockchain {
    
using namespace boost;

block_pool::block_pool(size_t maximum_depth)
  : maximum_depth_(maximum_depth)
{
}

void block_pool::add(block_const_ptr block)
{
    // The block must be successfully validated.
    BITCOIN_ASSERT(!block->validation.error);
    block_entry entry{ block };

    BITCOIN_ASSERT(block->validation.state);
    auto height = block->header().validation.height;
    const auto& left = blocks_.left;

    // Caller must ensure the entry does not exist.
    BITCOIN_ASSERT(left.find(entry) == left.end());

    // Add a back pointer from the parent for clearing the path later.
    const block_entry parent_entry{ block->header().previous_block_hash() };
    const auto parent = left.find(parent_entry);

    if (parent != left.end())
    {
        height = 0;
        parent->first.add_child(block);
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    blocks_.insert({ std::move(entry), height });
    ///////////////////////////////////////////////////////////////////////////
}

void block_pool::add(block_const_ptr_list_const_ptr blocks)
{
    const auto insert = [&](const block_const_ptr& block) { add(block); };
    std::for_each(blocks->begin(), blocks->end(), insert);
}

void block_pool::remove(block_const_ptr_list_const_ptr blocks)
{
    // The blocks list is expected to end with the new block (not in the pool).
    const auto last_pool_block = blocks->size() - 1u;
    auto& left = blocks_.left;

    for (size_t block = 0; block < last_pool_block; ++block)
    {
        auto it = left.find(block_entry{ (*blocks)[block]->hash() });
        BITCOIN_ASSERT(it != left.end());

        // There must be no children if this is the last pool block.
        // There must be at least one child if this is not the last pool block.
        DEBUG_ONLY(const auto last = (block == last_pool_block - 1u);)
        BITCOIN_ASSERT(last == it->first.children().empty());

        // Copy the block to a root node after deleting its next block child.
        if (it->first.children().size() > 1u)
        {
            auto entry_copy = it->first;
            const auto height = entry_copy.block()->header().validation.height;
            DEBUG_ONLY(const auto is_root_node = height != 0;)
            BITCOIN_ASSERT(is_root_node);

            // It cannot be the last block in blocks if there are children.
            // Remove child hash so next delete doesn't have to search for it.
            BITCOIN_ASSERT(block < entry_copy.children().size() - 1u);
            entry_copy.remove_child((*blocks)[block + 1u]);

            ///////////////////////////////////////////////////////////////////
            // Critical Section
            unique_lock lock(mutex_);
            left.erase(it);
            blocks_.insert({ std::move(entry_copy), height });
            ///////////////////////////////////////////////////////////////////
        }
        else
        {
            ///////////////////////////////////////////////////////////////////
            // Critical Section
            unique_lock lock(mutex_);
            left.erase(it);
            ///////////////////////////////////////////////////////////////////
        }
    }
}

// private
void block_pool::prune(block_entry::hashes&& hashes)
{
    block_entry::hashes child_hashes;
    auto saver = [&](const hash_digest& hash){ child_hashes.push_back(hash); };
    auto& left = blocks_.left;

    for (auto& hash: hashes)
    {
        const auto it = left.find(block_entry{ std::move(hash) });
        BITCOIN_ASSERT(it != left.end());

        // Save the children!
        const auto& children = it->first.children();
        std::for_each(children.begin(), children.end(), saver);

        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        unique_lock lock(mutex_);
        left.erase(it);
        ///////////////////////////////////////////////////////////////////////
    }

    // Reuse the iteration pattern from the top level prune.
    if (!child_hashes.empty())
        prune(std::move(child_hashes));
}

void block_pool::prune(size_t top_height)
{
    block_entry::hashes child_hashes;
    auto saver = [&](const hash_digest& hash){ child_hashes.push_back(hash); };
    auto& right = blocks_.right;

    // Height minus maximum depth is the minimum unpruned height.
    const auto minimum_height = floor_subtract(top_height, maximum_depth_);

    for (auto it = right.begin(); it != right.end() && it->first != 0 &&
        it->first < minimum_height;)
    {
        // Save the children!
        const auto& children = it->second.children();
        std::for_each(children.begin(), children.end(), saver);

        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        unique_lock lock(mutex_);
        it = right.erase(it);
        ///////////////////////////////////////////////////////////////////////
    }

    // The prevennts recursize erase inside of the iterator.
    if (!child_hashes.empty())
        prune(std::move(child_hashes));
}

void block_pool::filter(get_data_ptr message) const
{
    auto& inventories = message->inventories();
    const auto& left = blocks_.left;

    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (!it->is_block_type())
        {
            ++it;
            continue;
        }

        const block_entry entry{ it->hash() };

        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        mutex_.lock_shared();
        const auto found = (left.find(entry) != left.end());
        mutex_.unlock_shared();
        ///////////////////////////////////////////////////////////////////////

        it = found ? inventories.erase(it) : it + 1;
    }
}

// private
bool block_pool::exists(block_const_ptr candidate_block) const
{
    // The block must not yet be successfully validated.
    BITCOIN_ASSERT(candidate_block->validation.error);
    const auto& left = blocks_.left;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);
    return left.find(block_entry{ candidate_block }) != left.end();
    ///////////////////////////////////////////////////////////////////////////
}

// private
block_const_ptr block_pool::parent(block_const_ptr block) const
{
    // The block may be validated (pool) or not (new).
    const block_entry parent_entry{ block->header().previous_block_hash() };
    const auto& left = blocks_.left;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);
    const auto parent = left.find(parent_entry);
    return parent == left.end() ? nullptr : parent->first.block();
    ///////////////////////////////////////////////////////////////////////////
}

branch::ptr block_pool::get_path(block_const_ptr block) const
{
    ////dump();

    const auto trace = std::make_shared<branch>();

    if (exists(block))
        return trace;

    while (block)
    {
        trace->push_front(block);
        block = parent(block);
    }

    return trace;
}

void block_pool::dump() const
{
    LOG_INFO(LOG_BLOCKCHAIN) << "pool: ";

    // Dump in hash order with height suffix (roots have height).
    for (const auto& entry: blocks_.left)
    {
        LOG_INFO(LOG_BLOCKCHAIN)
            << entry.first << " " << entry.second;
    }
}

} // namespace blockchain
} // namespace libbitcoin
