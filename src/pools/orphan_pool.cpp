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
#include <memory>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

namespace libbitcoin {
namespace blockchain {

orphan_pool::orphan_pool(size_t capacity)
  : capacity_(std::min(capacity, size_t(1))), sequence_(0)
{
}

bool orphan_pool::add(block_const_ptr block)
{
    // The block has passed static validation checks prior to this call.
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    // No pool duplicates allowed by block hash.
    if (buffer_.left.find(block) != buffer_.left.end())
    {
        mutex_.unlock_upgrade();
        //-----------------------------------------------------------------
        return false;
    }

    const auto size = buffer_.size();
    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // It's been a very long time since the last restart.
    if (sequence_ == max_size_t)
        buffer_.clear();

    // Remove the oldest entry if the buffer is at capacity.
    if (size == capacity_)
        buffer_.right.erase(buffer_.right.begin());

    buffer_.insert({ block, ++sequence_ });
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////LOG_DEBUG(LOG_BLOCKCHAIN)
    ////    << "Orphan pool added block [" << encode_hash(block->hash())
    ////    << "] previous [" << encode_hash(header.previous_block_hash())
    ////    << "] old size (" << size << ").";

    return true;
}

bool orphan_pool::add(block_const_ptr_list_const_ptr blocks)
{
    // TODO: Popped block prevalidation may not hold depending on collision.
    // These are blocks arriving from the blockchain, so are already validated.

    auto success = true;
    auto adder = [&](const block_const_ptr& block) { success &= add(block); };
    std::for_each(blocks->begin(), blocks->end(), adder);
    return success;
}

void orphan_pool::remove(block_const_ptr block)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    // Find the block entry based on the block hash function.
    const auto it = buffer_.left.find(block);

    if (it == buffer_.left.end())
    {
        mutex_.unlock_upgrade();
        //-----------------------------------------------------------------
        return;
    }

    ////const auto old_size = buffer_.size();
    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    buffer_.left.erase(it);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////LOG_DEBUG(LOG_BLOCKCHAIN)
    ////    << "Orphan pool removed block [" << encode_hash(block->hash())
    ////    << "] old size (" << old_size << ").";
}

void orphan_pool::remove(block_const_ptr_list_const_ptr blocks)
{
    auto remover = [&](const block_const_ptr& block) { remove(block); };
    std::for_each(blocks->begin(), blocks->end(), remover);
}

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

        const auto key = create_key(it->hash());

        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        mutex_.lock_shared();

        const auto found = (buffer_.left.find(key) != buffer_.left.end());

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

block_const_ptr orphan_pool::create_key(const hash_digest& hash)
{
    auto copy = hash;
    return create_key(copy);
}

block_const_ptr orphan_pool::create_key(hash_digest&& hash)
{
    // Construct a block_const_ptr key using header hash injection.
    return std::make_shared<const message::block>(message::block
    {
        chain::header{ chain::header{}, hash },
        chain::transaction::list{}
    });
}

} // namespace blockchain
} // namespace libbitcoin
