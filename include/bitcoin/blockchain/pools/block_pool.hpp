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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/block_entry.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API block_pool
{
public:
    block_pool(const settings& settings);

    /// The number of blocks in the pool.
    size_t size() const;

    /// Get the block from the pool if it exists.
    system::block_const_ptr get(size_t height) const;

    /// Add downloaded block.
    void add(system::block_const_ptr block, size_t height);

    /// Purge previosuly-confirmed blocks at and below the specified height.
    void prune(size_t height);

    /// Clear the pool.
    void clear();

    /////// Remove all message vectors that match block hashes.
    ////void filter(system::get_data_ptr message) const;

private:
    // A bidirectional map is used for efficient header and position retrieval.
    // This produces the effect of a circular buffer hash table header forest.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<block_entry>,
        boost::bimaps::multiset_of<size_t>> block_entries;

    // This is thread safe.
    const size_t maximum_size_;

    // This is protected by mutex.
    block_entries blocks_;
    mutable system::upgrade_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
