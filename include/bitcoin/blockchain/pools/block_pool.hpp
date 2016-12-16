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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_POOL_HPP

#include <cstddef>
#include <memory>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

// Standard (boost) hash.
//-----------------------------------------------------------------------------

namespace boost
{

// Extend boost namespace with our block_const_ptr hash function.
template <>
struct hash<bc::blockchain::block_const_ptr>
{
    size_t operator()(const bc::blockchain::block_const_ptr& block) const
    {
        return boost::hash<bc::hash_digest>()(block->hash());
    }
};

} // namespace boost

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// An unordered memory pool for orphan blocks.
class BCB_API block_pool
{
public:
    typedef std::shared_ptr<block_pool> ptr;
    typedef std::shared_ptr<const block_const_ptr_list>
        block_const_ptr_list_const_ptr;

    block_pool(size_t capacity);

    /// Add a block to the pool.
    bool add(block_const_ptr block);

    /// Add a set of blocks to the pool.
    bool add(block_const_ptr_list_const_ptr blocks);

    /// Remove a block from the pool.
    void remove(block_const_ptr block);

    /// Remove a set of blocks from the pool.
    void remove(block_const_ptr_list_const_ptr blocks);

    /// Remove from the message all vectors that match orphans.
    void filter(get_data_ptr message) const;

    /// Get the longest connected chain of orphans to block.
    fork::ptr trace(block_const_ptr block) const;

private:
    // A bidirection map is used for efficient block and position retrieval.
    // This produces the effect of a circular buffer hash table of blocks.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<block_const_ptr>,
        boost::bimaps::set_of<size_t>> block_blocks;

    static block_const_ptr create_key(hash_digest&& hash);
    static block_const_ptr create_key(const hash_digest& hash);

    // This is thread safe.
    const size_t capacity_;

    // These are protected by mutex.
    size_t sequence_;
    block_blocks buffer_;
    mutable upgrade_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
