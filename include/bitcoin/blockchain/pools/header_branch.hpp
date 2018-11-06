/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_BRANCH_HPP
#define LIBBITCOIN_BLOCKCHAIN_BRANCH_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
class BCB_API header_branch
{
public:
    typedef std::shared_ptr<header_branch> ptr;
    typedef std::shared_ptr<const header_branch> const_ptr;

    /// Establish a header_branch with the given parent height.
    header_branch(size_t height=max_size_t);

    /// Set the height of the parent of this branch (fork point).
    void set_fork_height(size_t height);

    /// Push the header onto the branch, true if chains to top.
    bool push(header_const_ptr header);

    /// The parent header of the top header of the branch, if both exist.
    header_const_ptr top_parent() const;

    /// The top header of the branch, if it exists.
    header_const_ptr top() const;

    /// The top header of the branch, if it exists.
    size_t top_height() const;

    /// The member header pointer list.
    header_const_ptr_list_const_ptr headers() const;

    /// True if there are any headers in the branch.
    bool empty() const;

    /// The number of headers in the branch.
    size_t size() const;

    /// Summarize the work of the branch.
    uint256_t work() const;

    /// The hash of the branch parent (fork point).
    hash_digest fork_hash() const;

    /// The height of the parent parent (fork point).
    size_t fork_height() const;

    /// The branch parent (fork point), identical to { hash(), height() }.
    config::checkpoint fork_point() const;

    /// The bits of the header at the given height in the branch.
    bool get_bits(uint32_t& out_bits, size_t height) const;

    /// The bits of the header at the given height in the branch.
    bool get_version(uint32_t& out_version, size_t height) const;

    /// The bits of the header at the given height in the branch.
    bool get_timestamp(uint32_t& out_timestamp, size_t height) const;

    /// The hash of the header at the given height if it exists in the branch.
    bool get_block_hash(hash_digest& out_hash, size_t height) const;

protected:
    size_t index_of(size_t height) const;
    size_t height_at(size_t index) const;

private:
    size_t height_;

    /// The chain of headers in the branch.
    header_const_ptr_list_ptr headers_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
