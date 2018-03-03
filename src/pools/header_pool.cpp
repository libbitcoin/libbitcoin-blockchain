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
#include <bitcoin/blockchain/pools/header_pool.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace boost;

header_pool::header_pool(size_t maximum_depth)
  : maximum_depth_(maximum_depth == 0 ? max_size_t : maximum_depth)
{
}

size_t header_pool::size() const
{
    return headers_.size();
}

// protected
bool header_pool::exists(const hash_digest& hash) const
{
    const auto& left = headers_.left;
    return left.find(header_entry{ hash }) != left.end();
}

bool header_pool::exists(header_const_ptr candidate_header) const
{
    // The header must not yet be successfully validated.
    ////BITCOIN_ASSERT(candidate_header->metadata.error);
    return exists(candidate_header->hash());
}

// TODO: as blocks are popped from the confirmed chain they are pushed here
// which can result in existing dependent branches becoming disconnected from
// those blocks. To prevent this existing branch roots must be reparented
// following a reorg. For each add query for root of next height and connect.
void header_pool::add(header_const_ptr valid_header, size_t height)
{
    // The header must be successfully validated.
    ////BITCOIN_ASSERT(!valid_header->metadata.error);
    header_entry entry{ valid_header, height };
    const auto& left = headers_.left;

    // Caller ensures the entry does not exist by using exists(), but
    // insert rejects the header if there is an entry of the same hash.
    ////BITCOIN_ASSERT(left.find(entry) == left.end());

    // Add a back pointer from the parent for clearing the path later.
    const header_entry parent{ valid_header->previous_block_hash() };
    const auto it = left.find(parent);

    // Add child and clear chain state from internal (parent) node.
    if (it != left.end())
    {
        height = 0;
        it->first.add_child(valid_header);
        it->first.header()->metadata.state.reset();
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    headers_.insert({ std::move(entry), height });
    ///////////////////////////////////////////////////////////////////////////
}

void header_pool::add(header_const_ptr_list_const_ptr valid_headers,
    size_t height)
{
    const auto insert = [&](const header_const_ptr& header)
    {
        add(header, height++);
    };

    std::for_each(valid_headers->begin(), valid_headers->end(), insert);
}

// The pool is a forest connected to the chain at the roots of each tree.
// We delete only roots, pulling the tree "down" as we go based on expiration
// or acceptance. So there is never internal removal of a node.
void header_pool::remove(header_const_ptr_list_const_ptr accepted_headers)
{
    hash_list child_hashes;
    auto saver = [&](const hash_digest& hash){ child_hashes.push_back(hash); };
    auto& left = headers_.left;

    for (auto header: *accepted_headers)
    {
        auto it = left.find(header_entry{ header->hash() });

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
        auto it = left.find(header_entry{ child });

        // Except for sub-branches all children should have been deleted above.
        if (it == left.end())
            continue;

        // Copy the entry so that it can be deleted and replanted with height.
        const auto copy = it->first;
        BITCOIN_ASSERT(it->second == 0);

        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        unique_lock lock(mutex_);
        left.erase(it);
        headers_.insert({ copy, copy.height() });
        ///////////////////////////////////////////////////////////////////////
    }
}

// protected
void header_pool::prune(const hash_list& hashes, size_t minimum_height)
{
    hash_list child_hashes;
    auto saver = [&](const hash_digest& hash){ child_hashes.push_back(hash); };
    auto& left = headers_.left;

    for (auto& hash: hashes)
    {
        const auto it = left.find(header_entry{ hash });
        BITCOIN_ASSERT(it != left.end());

        const auto height = it->first.height();

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
        headers_.insert({ copy, height });
        ///////////////////////////////////////////////////////////////////////
    }

    // Recurse the children to span the tree.
    if (!child_hashes.empty())
        prune(child_hashes, minimum_height);
}

void header_pool::prune(size_t top_height)
{
    hash_list hashes;
    const auto minimum_height = floor_subtract(top_height, maximum_depth_);

    // TODO: not using table sort here, should stop iterating once above min.
    // Iterate over all root nodes with insufficient height.
    for (auto it: headers_.right)
        if (it.first != 0 && it.first < minimum_height)
            hashes.push_back(it.second.hash());

    // Get outside of the hash table iterator before deleting.
    if (!hashes.empty())
        prune(hashes, minimum_height);
}

// This is guarded against concurrent write (the only reason for the mutex).
void header_pool::filter(get_data_ptr message) const
{
    auto& inventories = message->inventories();

    // TODO: optimize (prevent repeating vector erase moves).
    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (!it->is_block_type())
        {
            ++it;
            continue;
        }

        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        mutex_.lock_shared();
        const auto found = exists(it->hash());
        mutex_.unlock_shared();
        ///////////////////////////////////////////////////////////////////////

        it = (found ? inventories.erase(it) : it + 1);
    }
}

// protected
header_const_ptr header_pool::parent(header_const_ptr header) const
{
    // The header may be validated (pool) or not (new).
    const header_entry parent_entry{ header->previous_block_hash() };
    const auto& left = headers_.left;

    const auto parent = left.find(parent_entry);
    return parent == left.end() ? nullptr : parent->first.header();
}

header_branch::ptr header_pool::get_branch(header_const_ptr header) const
{
    ////log_content();
    const auto trace = std::make_shared<header_branch>();

    if (exists(header))
        return trace;

    while (header)
    {
        trace->push(header);
        header = parent(header);
    }

    return trace;
}

////// private
////void header_pool::log_content() const
////{
////    LOG_INFO(LOG_BLOCKCHAIN) << "pool: ";
////
////    // Dump in hash order with height suffix (roots have height).
////    for (const auto& entry: headers_.left)
////    {
////        LOG_INFO(LOG_BLOCKCHAIN)
////            << entry.first << " " << entry.second;
////    }
////}

} // namespace blockchain
} // namespace libbitcoin
