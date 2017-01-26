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
#ifndef LIBBITCOIN_BLOCKCHAIN_FORK_HPP
#define LIBBITCOIN_BLOCKCHAIN_FORK_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
class BCB_API branch
{
public:
    typedef std::shared_ptr<branch> ptr;
    typedef std::shared_ptr<const branch> const_ptr;

    /// Establish a branch with the given parent height.
    branch(size_t height=0);

    /// Set the height of the parent of this branch (fork point).
    void set_height(size_t height);

    /// Push the block onto the branch, true if successfully chains to parent.
    bool push_front(block_const_ptr block);

    /// The top block of the branch, if it exists.
    block_const_ptr top() const;

    /// The top block of the branch, if it exists.
    size_t top_height() const;

    /// Populate transaction validation state in the context of the branch.
    void populate_tx(const chain::transaction& tx) const;

    /// Populate prevout validation spend state in the context of the branch.
    void populate_spent(const chain::output_point& outpoint) const;

    /// Populate prevout validation output state in the context of the branch.
    void populate_prevout(const chain::output_point& outpoint) const;

    /// The member block pointer list.
    block_const_ptr_list_const_ptr blocks() const;

    /// Determine if there are any blocks in the branch.
    bool empty() const;

    /// The number of blocks in the branch.
    size_t size() const;

    /// Summarize the work of the branch.
    uint256_t work() const;

    /// The hash of the parent of this branch (branch point).
    hash_digest hash() const;

    /// The height of the parent of this branch (branch point).
    size_t height() const;

    /// A checkpoint of the fork point, identical to { hash(), height() }.
    config::checkpoint fork_point() const;

    /// The bits of the block at the given height in the branch.
    bool get_bits(uint32_t& out_bits, size_t height) const;

    /// The bits of the block at the given height in the branch.
    bool get_version(uint32_t& out_version, size_t height) const;

    /// The bits of the block at the given height in the branch.
    bool get_timestamp(uint32_t& out_timestamp, size_t height) const;

    /// The hash of the block at the given height if it exists in the branch.
    bool get_block_hash(hash_digest& out_hash, size_t height) const;

protected:
    size_t index_of(size_t height) const;
    size_t height_at(size_t index) const;

private:
    size_t height_;

    /// The chain of blocks in the branch.
    block_const_ptr_list_ptr blocks_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
