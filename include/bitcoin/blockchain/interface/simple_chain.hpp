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

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// A low level interface for encapsulation of the blockchain database.
/// Caller must ensure the database is not otherwise in use during these calls.
/// Implementations are NOT expected to be thread safe with the exception
/// that the import method may itself be called concurrently.
class BCB_API simple_chain
{
public:
    // Getters.
    // ------------------------------------------------------------------------

    /// Return the first and last gaps in the blockchain, or false if none.
    virtual bool get_gap_range(uint64_t& out_first,
        uint64_t& out_last) const = 0;

    /// Return the next chain gap at or after the specified start height.
    virtual bool get_next_gap(uint64_t& out_height,
        uint64_t start_height) const = 0;

    /// Get the difficulty of the branch starting at the given height.
    virtual bool get_difficulty(hash_number& out_difficulty,
        uint64_t from_height) const = 0;

    /// Get the header of the block at the given height.
    virtual bool get_header(chain::header& out_header,
        uint64_t height) const = 0;

    /// Get the height of the block with the given hash.
    virtual bool get_height(uint64_t& out_height,
        const hash_digest& block_hash) const = 0;

    /// Get the bits of the block with the given height.
    virtual bool get_bits(uint32_t& out_bits,
        const uint64_t& height) const = 0;

    /// Get the timestamp of the block with the given height.
    virtual bool get_timestamp(uint32_t& out_timestamp,
        const uint64_t& height) const = 0;

    /// Get the version of the block with the given height.
    virtual bool get_version(uint32_t& out_version,
        const uint64_t& height) const = 0;

    /// Get height of latest block.
    virtual bool get_last_height(uint64_t& out_height) const = 0;

    /// Get the hash digest of the transaction of the outpoint.
    virtual bool get_transaction_hash(hash_digest& out_hash,
        const chain::output_point& outpoint) const = 0;

    /// Get the block height of the transaction given its hash.
    virtual bool get_transaction_height(uint64_t& out_block_height,
        const hash_digest& transaction_hash) const = 0;

    /// Get the transaction of the given hash and its block height.
    virtual transaction_ptr get_transaction(uint64_t& out_block_height,
        const hash_digest& transaction_hash) const = 0;

    // Setters.
    // ------------------------------------------------------------------------

    /// Import a block for the given height.
    virtual bool import(block_const_ptr block, uint64_t height) = 0;

    /// Append the block to the top of the chain.
    virtual bool push(block_const_ptr block) = 0;

    /// Remove blocks at or above the given height, returning them in order.
    virtual bool pop_from(block_const_ptr_list& out_blocks,
        uint64_t height) = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
