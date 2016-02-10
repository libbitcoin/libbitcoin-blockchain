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
#ifndef LIBBITCOIN_BLOCKCHAIN_SIMPLE_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_SIMPLE_CHAIN_HPP

#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_detail.hpp>

namespace libbitcoin {
namespace blockchain {

/// This provides encapsulation for the blockchain database for the organizer.
class BCB_API simple_chain
{
public:
    typedef std::shared_ptr<simple_chain> ptr;

    /// Get the dificulty of a block at the given height.
    virtual hash_number get_difficulty(uint64_t height) = 0;

    /// Get the height of the given block.
    virtual bool get_height(uint64_t& out_height,
        const hash_digest& block_hash) = 0;

    /// Append the block to the top of the chain.
    virtual void push(block_detail::ptr block) = 0;

    /// Remove blocks at or above the given height, returning them in order.
    virtual bool pop_from(block_detail::list& out_blocks, uint64_t height) = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
