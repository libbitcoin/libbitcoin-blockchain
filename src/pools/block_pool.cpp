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

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

// Make thread safe for add/remove (distinct add/remove mutexes).
//
// Add blocks under blockchain populated critical section.
// Remove under blockchain candidated critical section.
//
// Limit to configured entry count (size) when adding.
// Clear heights at/below new add height to mitigate turds.
//
// Trigger read-ahead population when reading and below configured count
// while the max height is below the current populated top candidate and
// not currently reading ahead. Fan out read-ahead modulo network cores.

block_pool::block_pool(const settings& settings)
  : maximum_size_(settings.block_buffer_limit)
{
}

size_t block_pool::size() const
{
    return blocks_.size();
}

block_const_ptr block_pool::get(size_t height) const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);
    ////const auto it = blocks_.find(height);
    ////return it == blocks_.end() ? nullptr : it->second;
    ///////////////////////////////////////////////////////////////////////////
    return {};
}

void block_pool::add(block_const_ptr block, size_t height)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    ////blocks_.insert({ std::move(entry), height });
    ///////////////////////////////////////////////////////////////////////////
}

// TODO: prune all after.
void block_pool::prune(size_t height)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    ////blocks_.erase(height);
    ///////////////////////////////////////////////////////////////////////////
}

void block_pool::clear()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);
    blocks_.clear();
    ///////////////////////////////////////////////////////////////////////////
}

////void block_pool::filter(get_data_ptr message) const
////{
////}

} // namespace blockchain
} // namespace libbitcoin
