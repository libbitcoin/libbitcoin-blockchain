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
#ifndef LIBBITCOIN_BLOCKCHAIN_FAST_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_FAST_CHAIN_HPP

#include <cstddef>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>

namespace libbitcoin {
namespace blockchain {

/// A low level interface for encapsulation of the blockchain database.
/// Caller must ensure the database is not otherwise in use during these calls.
/// Implementations are NOT expected to be thread safe with the exception
/// that the import method may itself be called concurrently.
class BCB_API fast_chain
{
public:
    // This avoids conflict with the result_handler in safe_chain.
    typedef handle0 complete_handler;

    // Readers.
    // ------------------------------------------------------------------------
    // Thread safe.

    /// Get the highest confirmed block of the header index.
    virtual size_t get_fork_point() const = 0;

    /// Get top block or header-indexed header.
    virtual bool get_top(chain::header& out_header, size_t& out_height,
        bool block_index) const = 0;

    /// Get highest block or header index checkpoint.
    virtual bool get_top(config::checkpoint& out_checkpoint,
        bool block_index) const = 0;

    /// Get height of highest block in the block or header index.
    virtual bool get_top_height(size_t& out_height,
        bool block_index) const = 0;

    /// Get the block or header-indexed header by height.
    virtual bool get_header(chain::header& out_header, size_t height,
        bool block_index) const = 0;

    /// Get the block or header-indexed header by hash.
    virtual bool get_header(chain::header& out_header, size_t& out_height,
        const hash_digest& block_hash, bool block_index) const = 0;

    /// False if the block is not pending (for caller loop).
    virtual bool get_pending_block_hash(hash_digest& out_hash, bool& out_empty,
        size_t height) const = 0;

    /// Get the hash of the block at the given index height.
    virtual bool get_block_hash(hash_digest& out_hash,
        size_t height, bool block_index) const = 0;

    /// Get the cached error result code of a cached invalid block.
    virtual bool get_block_error(code& out_error,
        const hash_digest& block_hash) const = 0;

    /// Get the cached error result code of a cached invalid transaction.
    virtual bool get_transaction_error(code& out_error,
        const hash_digest& tx_hash) const = 0;

    /// Get the bits of the block with the given index height.
    virtual bool get_bits(uint32_t& out_bits, size_t height,
        bool block_index) const = 0;

    /// Get the timestamp of the block with the given index height.
    virtual bool get_timestamp(uint32_t& out_timestamp, size_t height,
        bool block_index) const = 0;

    /// Get the version of the block with the given index height.
    virtual bool get_version(uint32_t& out_version, size_t height,
        bool block_index) const = 0;

    /// Get the work of blocks above the given index height.
    virtual bool get_work(uint256_t& out_work, const uint256_t& maximum,
        size_t above_height, bool block_index) const = 0;

    /// Populate metadata of the given block header.
    virtual void populate_header(const chain::header& header,
        size_t fork_height=max_size_t) const = 0;

    /// Populate metadata of the given transaction.
    /// Sets metadata based on fork point, ignore indexing if max fork point.
    virtual void populate_transaction(const chain::transaction& tx,
        uint32_t forks, size_t fork_height=max_size_t) const = 0;

    /// Populate output and metadata of the output referenced by the outpoint.
    /// Sets metadata based on fork point and confirmation requirement.
    virtual void populate_output(const chain::output_point& outpoint,
        size_t fork_height=max_size_t) const = 0;

    /// Get the state of the given block (flags).
    virtual uint8_t get_block_state(const hash_digest& block_hash) const = 0;

    /// Get the state of the given transaction.
    virtual database::transaction_state get_transaction_state(
        const hash_digest& tx_hash) const = 0;

    // Writers.
    // ------------------------------------------------------------------------

    /// Push a validated header branch to the header index.
    virtual bool reindex(const config::checkpoint& fork_point,
        header_const_ptr_list_const_ptr incoming,
        header_const_ptr_list_ptr outgoing) = 0;

    /// Push an validated transaction to the tx table and index outputs.
    virtual bool push(transaction_const_ptr tx) = 0;

    /// Push a block to the blockchain, height is validated.
    virtual bool push(block_const_ptr block, size_t height,
        uint32_t median_time_past) = 0;

    // Properties
    // ------------------------------------------------------------------------

    /// Get chain state for header pool.
    virtual chain::chain_state::ptr header_pool_state() const = 0;

    /// Get chain state for transaction pool.
    virtual chain::chain_state::ptr transaction_pool_state() const = 0;

    /// Get chain state for the given indexed header.
    virtual chain::chain_state::ptr chain_state(const chain::header& header,
        size_t height) const = 0;

    /// Promote chain state from the given parent header.
    virtual chain::chain_state::ptr promote_state(const chain::header& header,
        chain::chain_state::ptr parent) const = 0;

    /// Promote chain state for the last header in the multi-header branch.
    virtual chain::chain_state::ptr promote_state(
        header_branch::const_ptr branch) const = 0;

    /// True if the top block age exceeds the configured limit.
    virtual bool is_blocks_stale() const = 0;

    /// True if the top header age exceeds the configured limit.
    virtual bool is_headers_stale() const = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
