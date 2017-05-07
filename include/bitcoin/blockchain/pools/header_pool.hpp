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
#ifndef LIBBITCOIN_BLOCKCHAIN_HEADER_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_HEADER_POOL_HPP

#include <cstddef>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/block_entry.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe against concurrent filtering only.
/// There is no search within blocks of the block pool (just hashes).
/// The branch object contains chain query for new (leaf) block validation.
/// All pool blocks are valid, lacking only sufficient work for reorganzation.
class BCB_API header_pool
{
public:
    header_pool(size_t maximum_depth);

    // The number of blocks in the pool.
    size_t size() const;

    /// Add newly-validated block (work insufficient to reorganize).
    void add(block_const_ptr valid_block);

    /// Add root path of reorganized blocks (no branches).
    void add(block_const_ptr_list_const_ptr valid_blocks);

    /// Remove path of accepted blocks (sub-branches moved to root).
    void remove(block_const_ptr_list_const_ptr accepted_blocks);

    /// Purge branches rooted below top minus maximum depth.
    void prune(size_t top_height);

    /// Remove all message vectors that match block hashes.
    void filter(get_data_ptr message) const;

    /// Get the root path to and including the new block.
    /// This will be empty if the block already exists in the pool.
    branch::ptr get_path(block_const_ptr candidate_block) const;

protected:
    // A bidirectional map is used for efficient block and position retrieval.
    // This produces the effect of a circular buffer hash table of blocks.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<block_entry>,
        boost::bimaps::multiset_of<size_t>> block_entries;

    void prune(const hash_list& hashes, size_t minimum_height);
    bool exists(block_const_ptr candidate_block) const;
    block_const_ptr parent(block_const_ptr block) const;
    ////void log_content() const;

    // This is thread safe.
    const size_t maximum_depth_;

    // This is guarded against filtering concurrent to writing.
    block_entries blocks_;
    mutable upgrade_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
