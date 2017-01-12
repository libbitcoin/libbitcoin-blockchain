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

size_t block_pool::size() const
{
    return blocks_.size();
}

void block_pool::add(block_const_ptr valid_block)
{
    // The block must be successfully validated.
    ////BITCOIN_ASSERT(!block->validation.error);
    block_entry entry{ valid_block };

    ////BITCOIN_ASSERT(block->validation.state);
    auto height = valid_block->header().validation.height;
    const auto& left = blocks_.left;

    // Caller ensure the entry does not exist by using get_path, but
    // insert rejects the block if there is an entry of the same hash.
    ////BITCOIN_ASSERT(left.find(entry) == left.end());

    // Add a back pointer from the parent for clearing the path later.
    const block_entry parent{ valid_block->header().previous_block_hash() };
    const auto it = left.find(parent);

    if (it != left.end())
    {
        height = 0;
        it->first.add_child(valid_block);
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    blocks_.insert({ std::move(entry), height });
    ///////////////////////////////////////////////////////////////////////////
}

void block_pool::add(block_const_ptr_list_const_ptr valid_blocks)
{
    const auto insert = [&](const block_const_ptr& block) { add(block); };
    std::for_each(valid_blocks->begin(), valid_blocks->end(), insert);
}

void block_pool::remove(block_const_ptr_list_const_ptr accepted_blocks)
{
    hash_list child_hashes;
    auto saver = [&](const hash_digest& hash){ child_hashes.push_back(hash); };
    auto& left = blocks_.left;

    for (auto block: *accepted_blocks)
    {
        auto it = left.find(block_entry{ block->hash() });

        if (it == left.end())
            continue;

        // Copy hashes of all children of nodes we delete.
        const auto& children = it->first.children();
        std::for_each(children.begin(), children.end(), saver);

        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        unique_lock lock(mutex_);
        left.erase(it);
        ///////////////////////////////////////////////////////////////////////
    }

    // Move all children that we have orphaned to the root (give them height).
    for (auto child: child_hashes)
    {
        auto it = left.find(block_entry{ child });

        // Except for sub-branches all children should have been deleted above.
        if (it == left.end())
            continue;

        // Copy the entry so that it can be deleted and replanted with height.
        const auto copy = it->first;
        const auto height = copy.block()->header().validation.height;
        BITCOIN_ASSERT(it->second == 0);

        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        unique_lock lock(mutex_);
        left.erase(it);
        blocks_.insert({ copy, height });
        ///////////////////////////////////////////////////////////////////////
    }
}

// protected
void block_pool::prune(const hash_list& hashes, size_t minimum_height)
{
    hash_list child_hashes;
    auto saver = [&](const hash_digest& hash){ child_hashes.push_back(hash); };
    auto& left = blocks_.left;

    for (auto& hash: hashes)
    {
        const auto it = left.find(block_entry{ hash });
        BITCOIN_ASSERT(it != left.end());

        const auto height = it->first.block()->header().validation.height;

        // Delete all roots and expired non-roots and recurse their children.
        if (it->second != 0 || height < minimum_height)
        {
            // delete
            const auto& children = it->first.children();
            std::for_each(children.begin(), children.end(), saver);

            ///////////////////////////////////////////////////////////////////
            // Critical Section
            unique_lock lock(mutex_);
            left.erase(it);
            ///////////////////////////////////////////////////////////////////

            continue;
        }

        // Copy the entry so that it can be deleted and replanted with height.
        const auto copy = it->first;

        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        unique_lock lock(mutex_);
        left.erase(it);
        blocks_.insert({ copy, height });
        ///////////////////////////////////////////////////////////////////////
    }

    // Recurse the children to span the tree.
    if (!child_hashes.empty())
        prune(child_hashes, minimum_height);
}

void block_pool::prune(size_t top_height)
{
    hash_list hashes;
    const auto minimum_height = floor_subtract(top_height, maximum_depth_);

    // Iterate over all root nodes with insufficient height.
    for (auto it: blocks_.right)
        if (it.first != 0 && it.first < minimum_height)
            hashes.push_back(it.second.hash());

    // Get outside of the hash table iterator before deleting.
    if (!hashes.empty())
        prune(hashes, minimum_height);
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

// protected
bool block_pool::exists(block_const_ptr candidate_block) const
{
    // The block must not yet be successfully validated.
    ////BITCOIN_ASSERT(candidate_block->validation.error);
    const auto& left = blocks_.left;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);
    return left.find(block_entry{ candidate_block }) != left.end();
    ///////////////////////////////////////////////////////////////////////////
}

// protected
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
    ////log_content();
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

////// private
////void block_pool::log_content() const
////{
////    LOG_INFO(LOG_BLOCKCHAIN) << "pool: ";
////
////    // Dump in hash order with height suffix (roots have height).
////    for (const auto& entry: blocks_.left)
////    {
////        LOG_INFO(LOG_BLOCKCHAIN)
////            << entry.first << " " << entry.second;
////    }
////}

} // namespace blockchain
} // namespace libbitcoin
