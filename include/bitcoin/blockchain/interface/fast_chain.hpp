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

    /// Get top confirmed or candidate header.
    virtual bool get_top(chain::header& out_header, size_t& out_height,
        bool candidate) const = 0;

    /// Get highest confirmed or candidate checkpoint.
    virtual bool get_top(config::checkpoint& out_checkpoint,
        bool candidate) const = 0;

    /// Get height of highest confirmed or candidate header.
    virtual bool get_top_height(size_t& out_height, bool candidate) const = 0;

    /// Get confirmed or candidate header by height.
    virtual bool get_header(chain::header& out_header, size_t height,
        bool candidate) const = 0;

    /// Get confirmed or candidate header by hash.
    virtual bool get_header(chain::header& out_header, size_t& out_height,
        const hash_digest& block_hash, bool candidate) const = 0;

    /// Get hash of the confirmed or candidate block by index height.
    virtual bool get_block_hash(hash_digest& out_hash, size_t height,
        bool candidate) const = 0;

    /// Get the cached error result code of a cached invalid block.
    virtual bool get_block_error(code& out_error,
        const hash_digest& block_hash) const = 0;

    /// Get bits of the confirmed or candidate block by index height.
    virtual bool get_bits(uint32_t& out_bits, size_t height,
        bool candidate) const = 0;

    /// Get timestamp of the confirmed or candidate block by index height.
    virtual bool get_timestamp(uint32_t& out_timestamp, size_t height,
        bool candidate) const = 0;

    /// Get version of the confirmed or candidate block by index height.
    virtual bool get_version(uint32_t& out_version, size_t height,
        bool candidate) const = 0;

    /// Get work of the confirmed or candidate block by index height.
    virtual bool get_work(uint256_t& out_work, const uint256_t& overcome,
        size_t above_height, bool candidate) const = 0;

    /// Get the block hash of an empty block, or false if missing or invalid.
    virtual bool get_downloadable(hash_digest& out_hash,
        size_t height) const = 0;

    /// Populate metadata of the given block header.
    virtual void populate_header(const chain::header& header) const = 0;

    /// Sets metadata based on fork point. 
    /// Populate metadata of the given transaction for block inclusion.
    virtual void populate_block_transaction(const chain::transaction& tx,
        uint32_t forks, size_t fork_height) const = 0;

    /// Populate metadata of the given transaction for pool inclusion.
    virtual void populate_pool_transaction(const chain::transaction& tx,
        uint32_t forks) const = 0;

    /// Sets metadata based on fork point. 
    /// Get the output that is referenced by the outpoint.
    virtual void populate_output(const chain::output_point& outpoint,
        size_t fork_height, bool candidate) const = 0;

    /// Get state (flags) of candidate or confirmed block by height.
    virtual uint8_t get_block_state(size_t height, bool candidate) const = 0;

    /// Get state (flags) of the given block by hash.
    virtual uint8_t get_block_state(const hash_digest& block_hash) const = 0;

    /// Get populated confirmed or candidate header by height (or null).
    virtual header_const_ptr get_header(size_t height,
        bool candidate) const = 0;

    /// Get populated confirmed or candidate block by height (or null).
    virtual block_const_ptr get_block(size_t height, bool witness,
        bool candidate) const = 0;

    // Writers.
    // ------------------------------------------------------------------------

    /// Store unconfirmed tx that was verified with the given forks.
    virtual code store(transaction_const_ptr tx) = 0;

    /// Reorganize the header index to the specified fork point.
    virtual code reorganize(const config::checkpoint& fork,
        header_const_ptr_list_const_ptr incoming) = 0;

    /// Update the stored block with txs.
    virtual code update(block_const_ptr block, size_t height) = 0;

    /// Set the block validation state.
    virtual code invalidate(const chain::header& header,
        const code& error) = 0;

    /// Set the block validation state and all candidate chain ancestors.
    virtual code invalidate(block_const_ptr block, size_t height) = 0;

    /// Set the block validation state and mark spent outputs.
    virtual code candidate(block_const_ptr block) = 0;

    /// Reorganize the block index to the fork point.
    virtual code reorganize(block_const_ptr_list_const_ptr branch_cache,
        size_t branch_height) = 0;

    // Properties
    // ------------------------------------------------------------------------

    /// Highest common block between candidate and confirmed chains.
    virtual config::checkpoint fork_point() const = 0;

    /// Get chain state for top candidate block (may not be valid).
    virtual chain::chain_state::ptr top_candidate_state() const = 0;

    /// Get chain state for top valid candidate (may be higher confirmeds).
    virtual chain::chain_state::ptr top_valid_candidate_state() const = 0;

    /// Get chain state for transaction pool (top confirmed plus one).
    virtual chain::chain_state::ptr next_confirmed_state() const = 0;

    /// True if the top candidate age exceeds the configured limit.
    virtual bool is_candidates_stale() const = 0;

    /// True if the top valid candidate age exceeds the configured limit.
    virtual bool is_validated_stale() const = 0;

    /// True if the top block age exceeds the configured limit.
    virtual bool is_blocks_stale() const = 0;

    /// The candidate chain has greater valid work than the confirmed chain.
    virtual bool is_reorganizable() const = 0;

    // Chain State
    // ------------------------------------------------------------------------

    /// Get chain state for the given indexed header.
    virtual chain::chain_state::ptr chain_state(const chain::header& header,
        size_t height) const = 0;

    /// Promote chain state from the given parent header.
    virtual chain::chain_state::ptr promote_state(const chain::header& header,
        chain::chain_state::ptr parent) const = 0;

    /// Promote chain state for the last header in the multi-header branch.
    virtual chain::chain_state::ptr promote_state(
        header_branch::const_ptr branch) const = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
