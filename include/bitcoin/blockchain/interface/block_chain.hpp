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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <vector>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/organizers/block_organizer.hpp>
#include <bitcoin/blockchain/organizers/header_organizer.hpp>
#include <bitcoin/blockchain/organizers/transaction_organizer.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// The fast_chain interface portion of this class is not thread safe.
class BCB_API block_chain
  : public safe_chain, public fast_chain, noncopyable
{
public:
    /// Relay transactions is network setting that is passed through to block
    /// population as an optimization. This can be removed once there is an
    /// in-memory cache of tx pool metadata, as the costly query will go away.
    block_chain(threadpool& pool,
        const blockchain::settings& chain_settings,
        const database::settings& database_settings);

    /// The database is closed on destruct, threads must be joined.
    ~block_chain();

    // ========================================================================
    // FAST CHAIN
    // ========================================================================

    // Readers.
    // ------------------------------------------------------------------------
    // Thread safe.

    /// Get the block hash of an empty block, or false if missing or not empty.
    bool get_if_empty(hash_digest& out_hash, size_t height) const;

    /// Get top block or header-indexed header.
    bool get_top(chain::header& out_header, size_t& out_height,
        bool block_index) const;

    /// Get highest block or header index checkpoint.
    bool get_top(config::checkpoint& out_checkpoint, bool block_index) const;

    /// Get height of highest block in the block or header index.
    bool get_top_height(size_t& out_height, bool block_index) const;

    /// Get the block or header-indexed header by height.
    bool get_header(chain::header& out_header, size_t height,
        bool block_index) const;

    /// Get the block or header-indexed header by hash.
    bool get_header(chain::header& out_header, size_t& out_height,
        const hash_digest& block_hash, bool block_index) const;

    /// Get the hash of the block at the given index height.
    bool get_block_hash(hash_digest& out_hash, size_t height,
        bool block_index) const;

    /// Get the cached error result code of a cached invalid block.
    bool get_block_error(code& out_error, const hash_digest& block_hash) const;

    /// Get the cached error result code of a cached invalid transaction.
    bool get_transaction_error(code& out_error,
        const hash_digest& tx_hash) const;

    /// Get the bits of the block with the given index height.
    bool get_bits(uint32_t& out_bits, size_t height,
        bool block_index) const;

    /// Get the timestamp of the block with the given index height.
    bool get_timestamp(uint32_t& out_timestamp, size_t height,
        bool block_index) const;

    /// Get the version of the block with the given index height.
    bool get_version(uint32_t& out_version, size_t height,
        bool block_index) const;

    /// Get the work of valid blocks above the given index height.
    bool get_work(uint256_t& out_work, const uint256_t& overcome,
        size_t above_height, bool block_index) const;

    /// Populate metadata of the given block header.
    void populate_header(const chain::header& header) const;

    /// Populate metadata of the given transaction for block inclusion.
    void populate_block_transaction(const chain::transaction& tx,
        uint32_t forks, size_t fork_height) const;

    /// Populate metadata of the given transaction for pool inclusion.
    void populate_pool_transaction(const chain::transaction& tx,
        uint32_t forks) const;

    /// Get the output that is referenced by the outpoint.
    /// Sets metadata based on fork point. 
    void populate_output(const chain::output_point& outpoint,
        size_t fork_height, bool candidate) const;

    /// Get the state of the given block (flags).
    uint8_t get_block_state(size_t height, bool block_index) const;

    /// Get the state of the given block (flags).
    uint8_t get_block_state(const hash_digest& block_hash) const;

    /// Get the state of the given transaction.
    database::transaction_state get_transaction_state(
        const hash_digest& tx_hash) const;

    /// Get the populated header by indexed|confirmed height (or null).
    header_const_ptr get_header(size_t height, bool block_index) const;

    /// Get the populated block by indexed|confirmed height (or null).
    block_const_ptr get_block(size_t height, bool witness,
        bool block_index) const;

    // Writers.
    // ------------------------------------------------------------------------

    /// Store unconfirmed tx that was verified with the given forks.
    code store(transaction_const_ptr tx);

    /// Reorganize the header index to fork point, mark/unmark index spends.
    code reorganize(const config::checkpoint& fork,
        header_const_ptr_list_const_ptr incoming);

    /// Update the stored block with txs.
    code update(block_const_ptr block, size_t height);

    /// Set the block validation state.
    code invalidate(block_const_ptr block, size_t height);

    /// Set the block validation state and mark spent outputs.
    code candidate(block_const_ptr block);

    /// Reorganize the block index to the fork point, unmark index spends.
    code reorganize(block_const_ptr_list_const_ptr branch_cache,
        size_t branch_height);

    // Properties
    // ------------------------------------------------------------------------

    /// Get chain state for top candidate block (may not be valid).
    chain::chain_state::ptr top_candidate_state() const;

    /// Get chain state for top valid candidate (may be higher confirmeds).
    chain::chain_state::ptr top_valid_candidate_state() const;

    /// Get chain state for transaction pool (top confirmed plus one).
    chain::chain_state::ptr next_confirmed_state() const;

    /// True if the top candidate age exceeds the configured limit.
    bool is_candidates_stale() const;

    /// True if the top valid candidate age exceeds the configured limit.
    bool is_validated_stale() const;

    /// True if the top block age exceeds the configured limit.
    bool is_blocks_stale() const;

    /// The candidate chain has greater valid work than the confirmed chain.
    bool is_reorganizable() const;

    // Chain State
    // ------------------------------------------------------------------------

    /// Get chain state for the given indexed header.
    chain::chain_state::ptr chain_state(const chain::header& header,
        size_t height) const;

    /// Promote chain state from the given parent header.
    chain::chain_state::ptr promote_state(const chain::header& header,
        chain::chain_state::ptr parent) const;

    /// Promote chain state for the last header in the multi-header branch.
    chain::chain_state::ptr promote_state(header_branch::const_ptr branch) const;

    // ========================================================================
    // SAFE CHAIN
    // ========================================================================
    // Thread safe.

    // Startup and shutdown.
    // ------------------------------------------------------------------------
    // Thread safe except start.

    /// Start the block pool and the transaction pool.
    bool start();

    /// Signal pool work stop, speeds shutdown with multiple threads.
    bool stop();

    /// Unmaps all memory and frees the database file handles.
    /// Threads must be joined before close is called (or by destruct).
    bool close();

    // Node Queries.
    // ------------------------------------------------------------------------

    /// fetch a block by height.
    void fetch_block(size_t height, bool witness,
        block_fetch_handler handler) const;

    /// fetch a block by hash.
    void fetch_block(const hash_digest& hash, bool witness,
        block_fetch_handler handler) const;

    /// fetch block header by height.
    void fetch_block_header(size_t height,
        block_header_fetch_handler handler) const;

    /// fetch block header by hash.
    void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler) const;

    /// fetch hashes of transactions for a block, by block height.
    void fetch_merkle_block(size_t height,
        merkle_block_fetch_handler handler) const;

    /// fetch hashes of transactions for a block, by block hash.
    void fetch_merkle_block(const hash_digest& hash,
        merkle_block_fetch_handler handler) const;

    /// fetch compact block by block height.
    void fetch_compact_block(size_t height,
        compact_block_fetch_handler handler) const;

    /// fetch compact block by block hash.
    void fetch_compact_block(const hash_digest& hash,
        compact_block_fetch_handler handler) const;

    /// fetch height of block by hash.
    void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler) const;

    /// fetch height of latest block.
    void fetch_last_height(last_height_fetch_handler handler) const;

    /// fetch transaction by hash.
    void fetch_transaction(const hash_digest& hash, bool require_confirmed,
        bool witness, transaction_fetch_handler handler) const;

    /// fetch position and height within block of transaction by hash.
    void fetch_transaction_position(const hash_digest& hash,
        bool require_confirmed, transaction_index_fetch_handler handler) const;

    /// fetch the set of block hashes indicated by the block locator.
    void fetch_locator_block_hashes(get_blocks_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        inventory_fetch_handler handler) const;

    /// fetch the set of block headers indicated by the block locator.
    void fetch_locator_block_headers(get_headers_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const;

    /////// fetch an inventory locator relative to the current top and threshold.
    ////void fetch_block_locator(const chain::block::indexes& heights,
    ////    block_locator_fetch_handler handler) const;

    /// fetch a header locator relative to the current top and threshold.
    void fetch_header_locator(const chain::block::indexes& heights,
       header_locator_fetch_handler handler) const;

    // Server Queries.
    //-------------------------------------------------------------------------

    /// fetch the inpoint (spender) of an outpoint.
    void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler) const;

    /// fetch outputs, values and spends for an address_hash.
    void fetch_history(const short_hash& address_hash, size_t limit,
        size_t from_height, history_fetch_handler handler) const;

    /// fetch stealth results.
    void fetch_stealth(const binary& filter, size_t from_height,
        stealth_fetch_handler handler) const;

    // Transaction Pool.
    //-------------------------------------------------------------------------

    /// Fetch a merkle block for the maximal fee block template.
    void fetch_template(merkle_block_fetch_handler handler) const;

    /// Fetch an inventory vector for a rational "mempool" message response.
    void fetch_mempool(size_t count_limit, uint64_t minimum_fee,
        inventory_fetch_handler handler) const;

    // Filters.
    //-------------------------------------------------------------------------

    /// Filter inventory by block hash confirmed or pooled.
    void filter_blocks(get_data_ptr message, result_handler handler) const;

    /// Filter inventory by transaction confirmed and unconfirmed hash.
    void filter_transactions(get_data_ptr message,
        result_handler handler) const;

    // Subscribers.
    //-------------------------------------------------------------------------

    /// Subscribe to indexed header reorganizations, get branch/height.
    void subscribe_headers(reindex_handler&& handler);

    /// Subscribe to confirmed block reorganizations, get branch/height.
    void subscribe_blockchain(reorganize_handler&& handler);

    /// Subscribe to memory pool additions, get transaction.
    void subscribe_transaction(transaction_handler&& handler);

    /// Send null data success notification to all subscribers.
    void unsubscribe();

    // Organizers.
    //-------------------------------------------------------------------------

    /// Organize a header into the candidate chain and organize accordinly.
    void organize(header_const_ptr header, result_handler handler);

    /// Store a transaction to the pool.
    void organize(transaction_const_ptr tx, result_handler handler);

    /// Store a block's transactions and organize accordinly.
    code organize(block_const_ptr block, size_t height);

    // Properties.
    //-------------------------------------------------------------------------

    /// Get a reference to the blockchain configuration settings.
    const settings& chain_settings() const;

protected:

    /// Determine if work should terminate early with service stopped code.
    bool stopped() const;

private:
    // Utilities.
    //-------------------------------------------------------------------------

    config::checkpoint fork_point() const;
    uint256_t candidate_work() const;
    uint256_t confirmed_work() const;

    bool set_fork_point();
    bool set_candidate_work();
    bool set_confirmed_work();
    bool set_top_candidate_state();
    bool set_top_valid_candidate_state();
    bool set_next_confirmed_state();

    void set_fork_point(const config::checkpoint& fork);
    void set_candidate_work(const uint256_t& work_above_fork);
    void set_confirmed_work(const uint256_t& work_above_fork);
    void set_top_candidate_state(chain::chain_state::ptr top);
    void set_top_valid_candidate_state(chain::chain_state::ptr top);
    void set_next_confirmed_state(chain::chain_state::ptr top);

    bool get_transactions(chain::transaction::list& out_transactions,
        const database::block_result& result, bool witness) const;
    bool get_transaction_hashes(hash_list& out_hashes,
        const database::block_result& result) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const settings& settings_;

    // Atomic chain cache (thread safe).
    bc::atomic<uint256_t> candidate_work_;
    bc::atomic<uint256_t> confirmed_work_;
    bc::atomic<config::checkpoint> fork_point_;
    bc::atomic<block_const_ptr> last_block_;
    bc::atomic<transaction_const_ptr> last_transaction_;
    bc::atomic<chain::chain_state::ptr> top_candidate_state_;
    bc::atomic<chain::chain_state::ptr> top_valid_candidate_state_;
    bc::atomic<chain::chain_state::ptr> next_confirmed_state_;

    database::data_base database_;
    const populate_chain_state chain_state_populator_;
    const bool index_addresses_;

    // These are thread safe.
    mutable prioritized_mutex validation_mutex_;
    mutable threadpool priority_pool_;
    mutable dispatcher dispatch_;
    block_organizer block_organizer_;
    header_organizer header_organizer_;
    transaction_organizer transaction_organizer_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
