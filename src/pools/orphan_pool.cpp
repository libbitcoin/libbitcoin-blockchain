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
#include <bitcoin/blockchain/pools/orphan_pool.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <bitcoin/blockchain/validation/fork.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace std::placeholders;

orphan_pool::orphan_pool(size_t capacity)
  : capacity_(capacity == 0 ? 1 : capacity)
{
    buffer_.reserve(capacity_);
}

// There is no validation up to this point apart from deserialization.
bool orphan_pool::add(block_const_ptr block)
{
    const auto& header = block->header();

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    // No duplicates allowed.
    if (exists(header))
    {
        mutex_.unlock_upgrade();
        //-----------------------------------------------------------------
        return false;
    }

    const auto size = buffer_.size();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    mutex_.unlock_upgrade_and_lock();

    // Remove the front element, a circular buffer might be more efficient.
    if (size == capacity_)
        buffer_.erase(buffer_.begin());

    buffer_.push_back(block);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////log::debug(LOG_BLOCKCHAIN)
    ////    << "Orphan pool added block [" << encode_hash(block->hash())
    ////    << "] previous [" << encode_hash(header.previous_block_hash)
    ////    << "] old size (" << size << ").";

    return true;
}

// These are blocks arriving from the blockchain, so should be prevalidated.
bool orphan_pool::add(const block_const_ptr_list& blocks)
{
    auto adder = [this](const block_const_ptr& block) { add(block); };
    std::for_each(blocks.begin(), blocks.end(), adder);
}

void orphan_pool::remove(block_const_ptr block)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    const auto it = std::find(buffer_.begin(), buffer_.end(), block);

    if (it == buffer_.end())
    {
        mutex_.unlock_upgrade();
        //-----------------------------------------------------------------
        return;
    }

    ////const auto old_size = buffer_.size();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    mutex_.unlock_upgrade_and_lock();
    buffer_.erase(it);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////log::debug(LOG_BLOCKCHAIN)
    ////    << "Orphan pool removed block [" << encode_hash(block->hash())
    ////    << "] old size (" << old_size << ").";
}

void orphan_pool::remove(const block_const_ptr_list& blocks)
{
    auto remover = [this](const block_const_ptr& block) { remove(block); };
    std::for_each(blocks.begin(), blocks.end(), remover);
}

// TODO: use hash table pool to eliminate this O(n^2) search.
void orphan_pool::filter(get_data_ptr message) const
{
    auto& inventories = message->inventories();

    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (!it->is_block_type())
        {
            ++it;
            continue;
        }

        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        mutex_.lock_shared();
        const auto found = exists(it->hash());
        mutex_.unlock_shared();
        ///////////////////////////////////////////////////////////////////////

        it = found ? inventories.erase(it) : it + 1;
    }
}

fork::ptr orphan_pool::trace(block_const_ptr block) const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_shared();

    const auto capacity = buffer_.size();
    const auto trace = std::make_shared<fork>(capacity);

    ///////////////////////////////////////////////////////////////////////////
    // TODO: obtain longest possible chain containing the new block.
    ///////////////////////////////////////////////////////////////////////////
    trace->push(block);

    mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    return trace;
}

// private
//-----------------------------------------------------------------------------

bool orphan_pool::exists(const hash_digest& hash) const
{
    const auto match = [&hash](const block_const_ptr& entry)
    {
        return hash == entry->hash();
    };

    return std::any_of(buffer_.begin(), buffer_.end(), match);
}

bool orphan_pool::exists(const chain::header& header) const
{
    const auto match = [&header](const block_const_ptr& entry)
    {
        return header == entry->header();
    };

    return std::any_of(buffer_.begin(), buffer_.end(), match);
}

orphan_pool::const_iterator orphan_pool::find(const hash_digest& hash) const
{
    const auto match = [&hash](const block_const_ptr& entry)
    {
        return hash == entry->hash();
    };

    return std::find_if(buffer_.begin(), buffer_.end(), match);
}

} // namespace blockchain
} // namespace libbitcoin
