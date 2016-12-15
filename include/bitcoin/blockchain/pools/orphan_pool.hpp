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
#ifndef LIBBITCOIN_BLOCKCHAIN_ORPHAN_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_ORPHAN_POOL_HPP

#include <cstddef>
#include <memory>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// An unordered memory pool for orphan blocks.
class BCB_API orphan_pool
{
public:
    typedef std::shared_ptr<orphan_pool> ptr;
    typedef std::shared_ptr<const block_const_ptr_list>
        block_const_ptr_list_const_ptr;

    orphan_pool(size_t capacity);

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
    typedef std::vector<block_const_ptr> buffer;
    typedef buffer::const_iterator const_iterator;

    bool exists(const hash_digest& hash) const;
    const_iterator find(const hash_digest& hash) const;

    // The buffer is protected by mutex.
    buffer buffer_;
    mutable upgrade_mutex mutex_;
    const size_t capacity_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
