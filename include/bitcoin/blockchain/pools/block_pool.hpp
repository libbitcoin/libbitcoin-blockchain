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
#include <functional>
#include <unordered_set>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/block_entry.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API block_pool
{
public:
    typedef system::handle1<system::block_const_ptr> read_handler;
    typedef system::resubscriber<system::code, system::block_const_ptr,
        size_t> read_subscriber;

    // TODO: create common definition.
    typedef std::shared_ptr<system::block_const_ptr_list>
        block_const_ptr_list_ptr;

    block_pool(fast_chain& chain, const settings& settings);

    /// Start/stop the pool.
    bool start();
    bool stop();

    /// The number of blocks in the pool.
    size_t size() const;

    /// Add a block to the pool if it satisfies limits.
    void add(system::block_const_ptr block, size_t height);

    /// Add a set of blocks to the pool regardless of limits.
    void add(block_const_ptr_list_ptr blocks, size_t first_height);

    /// Get a block from the pool if cached otherwise from store if found.
    system::block_const_ptr get(size_t height);

    /// Fetch a block from the pool, reading it from store as required.
    /// Handler returns success code with empty pointer if not found.
    void fetch(size_t height, read_handler&& handler);

    /// Remove all message vectors that match block hashes.
    void filter(system::get_data_ptr message) const;

protected:
    /// Blocks are searchable by hash and height in order.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<block_entry>,
        boost::bimaps::set_of<size_t>> block_entries;

    /// A set of heights is maintained for blocks pending download.
    typedef std::unordered_set<size_t> block_heights;

    bool stopped() const;
    void read_block(size_t height);
    bool handle_add(const system::code& ec, system::block_const_ptr block,
        size_t height, size_t target_height, read_handler handler) const;

private:
    // These are protected by mutex.
    block_entries blocks_;
    block_heights pending_;
    mutable system::upgrade_mutex mutex_;

    // These are thread safe.
    fast_chain& chain_;
    std::atomic<bool> stopped_;
    const size_t maximum_size_;
    mutable system::threadpool pool_;
    mutable system::dispatcher dispatch_;
    read_subscriber::ptr subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
