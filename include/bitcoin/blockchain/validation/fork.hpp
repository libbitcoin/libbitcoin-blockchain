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
class BCB_API fork
{
public:
    typedef std::shared_ptr<fork> ptr;

    /// Establish a fork with the given parent checkpoint.
    fork(size_t capacity);

    /// Set the height of the parent of this fork (fork point).
    void set_height(size_t height);

    /// Push the block onto the fork, true if successfully chains to parent.
    bool push(block_const_ptr block);

    /// Pop the block and set the code, and all after to parent invalid.
    block_const_ptr_list pop(size_t index, const code& reason);

    /// Set validation result metadata on the block. 
    void set_verified(size_t index);

    /// Determine if the block has been validated for the height of index.
    bool is_verified(size_t index) const;

    /// The member block pointer list.
    const block_const_ptr_list& blocks() const;

    /// Clear the fork and reset its height to zero.
    void clear();

    /// Determine if there are any blocks in the fork.
    bool empty() const;

    /// The number of blocks in the fork.
    size_t size() const;

    /// The hash of the parent of this fork (fork point).
    hash_digest hash() const;

    /// The height of the parent of this fork (fork point).
    size_t height() const;

    /// The height of the block at the given index.
    size_t height_at(size_t index) const;

    /// The block at the given index.
    block_const_ptr block_at(size_t index) const;

    /// Summarize the difficulty of the fork.
    hash_number difficulty() const;

private:
    size_t height_;

    /// The chain of blocks in the fork.
    block_const_ptr_list blocks_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
