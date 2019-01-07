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
#include <bitcoin/blockchain/pools/block_pool.hpp>

#include <cstddef>
#include <utility>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/blockchain/pools/block_entry.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

// TODO: read-ahead population.
// Trigger read-ahead population when reading and below configured count
// while the max height is below the current populated top candidate and
// not currently reading ahead. Fan out read-ahead modulo network cores.

block_pool::block_pool(const settings& settings)
  : maximum_size_(settings.block_buffer_limit)
{
}

size_t block_pool::size() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);
    return blocks_.size();
    ///////////////////////////////////////////////////////////////////////////
}

// protected
bool block_pool::exists(const hash_digest& hash) const
{
    const auto& left = blocks_.left;
    return left.find(block_entry{ hash }) != left.end();
}

block_const_ptr block_pool::extract(size_t height)
{
    if (maximum_size_ == 0)
        return nullptr;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();
    auto& right = blocks_.right;
    const auto it = right.find(height);

    if (it == right.end())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return nullptr;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    const auto block = it->second.block();
    right.erase(it);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////LOG_DEBUG(LOG_BLOCKCHAIN)
    ////    << "Hit: " << height << " (" << blocks_.size() << ")";

    return block;
}

// Insert rejects entry if there is an entry of the same hash or height.
void block_pool::add(block_const_ptr block, size_t height, size_t top)
{
    if (maximum_size_ == 0)
        return;

    // Do not cache below or above scope blocks.
    if (height < top || (height - top) > maximum_size_)
        return;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock();
    blocks_.insert({ { block }, height });
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////LOG_DEBUG(LOG_BLOCKCHAIN)
    ////    << "Add: " << height << " (" << blocks_.size() << ")";
}

// TODO: invoke prune vs. erase in extract?
void block_pool::prune(size_t height)
{
    if (maximum_size_ == 0)
        return;

    // TODO: erase all blocks before the specified height.
    // TODO: use height sort to amortize the prune search cost.
}

void block_pool::clear()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    blocks_.clear();
    ///////////////////////////////////////////////////////////////////////////
}

void block_pool::filter(get_data_ptr message) const
{
    if (maximum_size_ == 0)
        return;

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

        it = (found ? inventories.erase(it) : std::next(it));
    }
}

} // namespace blockchain
} // namespace libbitcoin
