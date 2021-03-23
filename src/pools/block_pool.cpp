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
#include <functional>
#include <utility>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/block_entry.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;
using namespace std::placeholders;

#define NAME "block_pool"

block_pool::block_pool(fast_chain& chain, const settings& settings)
  : chain_(chain),
    stopped_(true),
    maximum_size_(settings.block_buffer_limit),

    // Create dispatcher for parallel read and subscriber for asynchrony.
    pool_(thread_ceiling(settings.cores), priority(settings.priority)),
    dispatch_(pool_, NAME "_dispatch"),
    subscriber_(std::make_shared<read_subscriber>(pool_, NAME))
{
}

// protected
bool block_pool::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool block_pool::start()
{
    stopped_ = false;
    subscriber_->start();
    return true;
}

bool block_pool::stop()
{
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, {}, 0);
    stopped_ = true;
    return true;
}

// Cached blocks.
//-----------------------------------------------------------------------------

size_t block_pool::size() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);
    return blocks_.size();
    ///////////////////////////////////////////////////////////////////////////
}

void block_pool::add(block_const_ptr_list_ptr blocks, size_t first_height)
{
    if (maximum_size_ == 0)
        return;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock();

    auto height = first_height;
    for (const auto& block: *blocks)
        blocks_.insert({ { block }, height++ });

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    height = first_height;
    for (const auto& block: *blocks)
        subscriber_->relay(error::success, block, height++);
}

// Insert rejects entry if there is an entry of the same hash or height.
void block_pool::add(block_const_ptr block, size_t height)
{
    if (maximum_size_ == 0)
        return;

    auto& cached = blocks_.right;
    const auto top_confirmed = chain_.fork_point().height();

    // Do not cache below or above scope blocks.
    // A pending download can't be purged but this preempts it.
    if (height > top_confirmed && (height - top_confirmed) <= maximum_size_)
    {
        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        mutex_.lock();
        blocks_.insert({ { block }, height });
        mutex_.unlock();
        ///////////////////////////////////////////////////////////////////////

        subscriber_->relay(error::success, block, height);
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////
    mutex_.lock();

    // Purge all cached blocks at and below the top_confirmed block.
    for (auto it = cached.begin();
        it != cached.end() && it->first <= top_confirmed;
        it = cached.erase(it)) {}

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////
}

block_const_ptr block_pool::get(size_t height)
{
    if (maximum_size_ == 0)
        return chain_.get_candidate(height);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();

    auto& cached = blocks_.right;
    const auto it = cached.find(height);

    if (it != cached.end())
    {
        const auto block = it->second.block();

        mutex_.unlock_upgrade_and_lock();
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        cached.erase(it);
        //---------------------------------------------------------------------
        mutex_.unlock_and_lock_upgrade();

        return block;
    }

    mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    return chain_.get_candidate(height);
}

void block_pool::fetch(size_t height, read_handler&& handler)
{
    // The cache is disabled, just read and return the block.
    if (maximum_size_ == 0)
    {
        handler(error::success, chain_.get_candidate(height));
        return;
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();

    auto& cached = blocks_.right;
    const auto it = cached.find(height);

    // If found remove block from the map and return it.
    if (it != cached.end())
    {
        const auto block = it->second.block();

        mutex_.unlock_upgrade_and_lock();
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        cached.erase(it);
        //---------------------------------------------------------------------
        mutex_.unlock();

        handler(error::success, block);
        return;
    }

    mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    // Since not found subscribe to block add  (for all blocks).
    subscriber_->subscribe(
        std::bind(&block_pool::handle_add,
            this, _1, _2, _3, height, handler), error::service_stopped, {}, 0);

    hash_digest unused;
    const auto top_confirmed = chain_.fork_point().height();

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();

    // Reads will queue in dispatcher until a read thread is free.
    for (auto next = height; next > top_confirmed &&
        (next - top_confirmed) <= maximum_size_; ++next)
    {
        // TODO: create optimal get_validatable replacement for this usage.
        // TODO: find a way for get_validatable outside of critical section.
        if (pending_.find(next) == pending_.end() &&
            cached.find(next) == cached.end() &&
            chain_.get_validatable(unused, next))
        {
            mutex_.unlock_upgrade_and_lock();
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            pending_.insert(next);
            //---------------------------------------------------------------------
            mutex_.unlock_and_lock_upgrade();

            dispatch_.concurrent(&block_pool::read_block, this, next);
        }
    }

    mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////
}

// protected
void block_pool::read_block(size_t height)
{
    const auto is_stopped = stopped();
    const auto ec = is_stopped ? error::service_stopped : error::success;

    // Block will be null if not populated, caller must test value.
    const auto block = is_stopped ? nullptr : chain_.get_candidate(height);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock();
    pending_.erase(height);
    blocks_.insert({ { block }, height });
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    subscriber_->relay(ec, block, height);
}

// protected
bool block_pool::handle_add(const code& ec, block_const_ptr block,
    size_t height, size_t target_height, read_handler handler) const
{
    if (ec)
        return false;

    if (height != target_height)
        return true;

    handler(ec, block);
    return false;
}

void block_pool::filter(get_data_ptr message) const
{
    if (maximum_size_ == 0)
        return;

    const auto& left = blocks_.left;
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
        const auto found = left.find(block_entry{ it->hash() }) != left.end();
        mutex_.unlock_shared();
        ///////////////////////////////////////////////////////////////////////

        it = (found ? inventories.erase(it) : std::next(it));
    }
}

} // namespace blockchain
} // namespace libbitcoin
