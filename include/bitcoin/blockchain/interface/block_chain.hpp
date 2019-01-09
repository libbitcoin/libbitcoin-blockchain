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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <vector>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/organizers/organize_block.hpp>
#include <bitcoin/blockchain/organizers/organize_header.hpp>
#include <bitcoin/blockchain/organizers/organize_transaction.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// The fast_chain interface portion of this class is not thread safe.
class BCB_API block_chain
  : public safe_chain, public fast_chain, system::noncopyable
{
public:
    typedef system::resubscriber<system::code, size_t,
        system::block_const_ptr_list_const_ptr,
        system::block_const_ptr_list_const_ptr> block_subscriber;
    typedef system::resubscriber<system::code, size_t,
        system::header_const_ptr_list_const_ptr,
        system::header_const_ptr_list_const_ptr> header_subscriber;
    typedef system::resubscriber<system::code, system::transaction_const_ptr>
        transaction_subscriber;

    /// Relay transactions is network setting that is passed through to block
    /// population as an optimization. This can be removed once there is an
    /// in-memory cache of tx pool metadata, as the costly query will go away.
    block_chain(system::threadpool& pool, const blockchain::settings& settings,
        const database::settings& database_settings,
        const system::settings& bitcoin_settings);

    /// The database is closed on destruct, threads must be joined.
    ~block_chain();

    // ========================================================================
    // FAST CHAIN
    // ========================================================================

    // Readers.
    // ------------------------------------------------------------------------
    // Thread safe.

    /// Get highest confirmed or candidate header.
    bool get_top(system::chain::header& out_header, size_t& out_height,
        bool candidate) const;

    /// Get highest confirmed or candidate checkpoint.
    bool get_top(system::config::checkpoint& out_checkpoint,
        bool candidate) const;

    /// Get height of highest confirmed or candidate header.
    bool get_top_height(size_t& out_height, bool candidate) const;

    /// Get confirmed or candidate header by height.
    bool get_header(system::chain::header& out_header, size_t height,
        bool candidate) const;

    /// Get confirmed or candidate header by hash.
    bool get_header(system::chain::header& out_header, size_t& out_height,
        const system::hash_digest& block_hash, bool candidate) const;

    /// Get hash of the confirmed or candidate block by index height.
    bool get_block_hash(system::hash_digest& out_hash, size_t height,
        bool candidate) const;

    /// Get the cached error result code of a cached invalid block.
    bool get_block_error(system::code& out_error,
        const system::hash_digest& block_hash) const;

    /// Get bits of the confirmed or candidate block by index height.
    bool get_bits(uint32_t& out_bits, size_t height,
        bool candidate) const;

    /// Get timestamp of the confirmed or candidate block by index height.
    bool get_timestamp(uint32_t& out_timestamp, size_t height,
        bool candidate) const;

    /// Get version of the confirmed or candidate block by index height.
    bool get_version(uint32_t& out_version, size_t height,
        bool candidate) const;

    /// Get work of the confirmed or candidate block by index height.
    bool get_work(system::uint256_t& out_work,
        const system::uint256_t& overcome, size_t above_height,
        bool candidate) const;

    /// Get block hash of an empty block, false if missing or failed.
    bool get_downloadable(system::hash_digest& out_hash, size_t height) const;

    /// Get block hash of an unvalidated block, false if empty/failed/valid.
    bool get_validatable(system::hash_digest& out_hash, size_t height) const;

    /// Push a validatable block identifier onto the download subscriber.
    void prime_validation(const system::hash_digest& hash,
        size_t height) const;

    /// Populate metadata of the given block header.
    void populate_header(const system::chain::header& header) const;

    /// Populate metadata of the given transaction for block inclusion.
    void populate_block_transaction(const system::chain::transaction& tx,
        uint32_t forks, size_t fork_height) const;

    /// Populate metadata of the given transaction for pool inclusion.
    void populate_pool_transaction(const system::chain::transaction& tx,
        uint32_t forks) const;

    /// Sets metadata based on fork point (no sentinel).
    /// Get the output that is referenced by the outpoint.
    bool populate_block_output(const system::chain::output_point& outpoint,
        size_t fork_height) const;

    /// Sets metadata for pool based on strong chain.
    /// Get the output that is referenced by the outpoint.
    bool populate_pool_output(
        const system::chain::output_point& outpoint) const;

    /// Get state (flags) of candidate or confirmed block by height.
    uint8_t get_block_state(size_t height, bool candidate) const;

    /// Get state (flags) of the given block by hash.
    uint8_t get_block_state(const system::hash_digest& block_hash) const;

    /// Get populated confirmed or candidate header by height (or null).
    system::header_const_ptr get_header(size_t height, bool candidate) const;

    /// Get populated candidate block by height with witness (or null).
    system::block_const_ptr get_candidate(size_t height) const;

    // Writers.
    // ------------------------------------------------------------------------

    /// Store unconfirmed tx that was verified with the given forks.
    system::code store(system::transaction_const_ptr tx);

    /// Reorganize the header index to fork point, mark/unmark index spends.
    system::code reorganize(const system::config::checkpoint& fork,
        system::header_const_ptr_list_const_ptr incoming);

    /// Update the stored block with txs.
    system::code update(system::block_const_ptr block, size_t height);

    /// Set the block validation state.
    system::code invalidate(const system::chain::header& header,
        const system::code& error);

    /// Set the block validation state and all candidate chain ancestors.
    system::code invalidate(system::block_const_ptr block, size_t height);

    /// Set the block validation state and mark spent outputs.
    system::code candidate(system::block_const_ptr block);

    /// Reorganize the block index to the fork point, unmark index spends.
    system::code reorganize(system::block_const_ptr_list_const_ptr branch_cache,
        size_t branch_height);

    // Properties
    // ------------------------------------------------------------------------

    /// Highest common block between candidate and confirmed chains.
    system::config::checkpoint fork_point() const;

    /// Get chain state for top candidate block (may not be valid).
    system::chain::chain_state::ptr top_candidate_state() const;

    /// Get chain state for top valid candidate (may be higher confirmeds).
    system::chain::chain_state::ptr top_valid_candidate_state() const;

    /// Get chain state for transaction pool (top confirmed plus one).
    system::chain::chain_state::ptr next_confirmed_state() const;

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
    system::chain::chain_state::ptr chain_state(
        const system::chain::header& header, size_t height) const;

    /// Promote chain state from the given parent header.
    system::chain::chain_state::ptr promote_state(
        const system::chain::header& header,
        system::chain::chain_state::ptr parent) const;

    /// Promote chain state for the last header in the multi-header branch.
    system::chain::chain_state::ptr promote_state(
        header_branch::const_ptr branch) const;

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
    void fetch_block(const system::hash_digest& hash, bool witness,
        block_fetch_handler handler) const;

    /// fetch block header by height.
    void fetch_block_header(size_t height,
        block_header_fetch_handler handler) const;

    /// fetch block header by hash.
    void fetch_block_header(const system::hash_digest& hash,
        block_header_fetch_handler handler) const;

    /// fetch hashes of transactions for a block, by block height.
    void fetch_merkle_block(size_t height,
        merkle_block_fetch_handler handler) const;

    /// fetch hashes of transactions for a block, by block hash.
    void fetch_merkle_block(const system::hash_digest& hash,
        merkle_block_fetch_handler handler) const;

    /// fetch compact block by block height.
    void fetch_compact_block(size_t height,
        compact_block_fetch_handler handler) const;

    /// fetch compact block by block hash.
    void fetch_compact_block(const system::hash_digest& hash,
        compact_block_fetch_handler handler) const;

    /// fetch height of block by hash.
    void fetch_block_height(const system::hash_digest& hash,
        block_height_fetch_handler handler) const;

    /// fetch height of latest block.
    void fetch_last_height(last_height_fetch_handler handler) const;

    /// fetch transaction by hash.
    void fetch_transaction(const system::hash_digest& hash,
        bool require_confirmed, bool witness,
        transaction_fetch_handler handler) const;

    /// fetch position and height within block of transaction by hash.
    void fetch_transaction_position(const system::hash_digest& hash,
        bool require_confirmed, transaction_index_fetch_handler handler) const;

    /// fetch the set of block hashes indicated by the block locator.
    void fetch_locator_block_hashes(system::get_blocks_const_ptr locator,
        const system::hash_digest& threshold, size_t limit,
        inventory_fetch_handler handler) const;

    /// fetch the set of block headers indicated by the block locator.
    void fetch_locator_block_headers(system::get_headers_const_ptr locator,
        const system::hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const;

    /////// fetch an inventory locator relative to the current top and threshold.
    ////void fetch_block_locator(const system::chain::block::indexes& heights,
    ////    block_locator_fetch_handler handler) const;

    /// fetch a header locator relative to the current top and threshold.
    void fetch_header_locator(const system::chain::block::indexes& heights,
       header_locator_fetch_handler handler) const;

    // Server Queries.
    //-------------------------------------------------------------------------

    /// fetch the inpoint (spender) of an outpoint.
    void fetch_spend(const system::chain::output_point& outpoint,
        spend_fetch_handler handler) const;

    /// fetch outputs, values and spends for an address_hash.
    void fetch_history(const system::short_hash& address_hash, size_t limit,
        size_t from_height, history_fetch_handler handler) const;

    /// fetch stealth results.
    void fetch_stealth(const system::binary& filter, size_t from_height,
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
    void filter_blocks(system::get_data_ptr message,
        result_handler handler) const;

    /// Filter inventory by transaction confirmed and unconfirmed hash.
    void filter_transactions(system::get_data_ptr message,
        result_handler handler) const;

    // Subscribers.
    //-------------------------------------------------------------------------

    /// Subscribe to confirmed block reorganizations, get branch/height.
    void subscribe_blocks(block_handler&& handler);

    /// Subscribe to indexed header reorganizations, get branch/height.
    void subscribe_headers(header_handler&& handler);

    /// Subscribe to memory pool additions, get transaction.
    void subscribe_transactions(transaction_handler&& handler);

    /// Send null data success notification to all subscribers.
    void unsubscribe();

    // Organizers.
    //-------------------------------------------------------------------------

    /// Store a block's transactions and organize accordingly.
    system::code organize(system::block_const_ptr block, size_t height);

    /// Organize a header into the candidate chain and organize accordingly.
    void organize(system::header_const_ptr header, result_handler handler);

    /// Store a transaction to the pool.
    void organize(system::transaction_const_ptr tx, result_handler handler);

    // Properties.
    //-------------------------------------------------------------------------

    /// Get a reference to the blockchain configuration settings.
    const settings& chain_settings() const;

protected:

    // Determine if work should terminate early with service stopped code.
    bool stopped() const;

    // Notification senders.
    void notify(system::transaction_const_ptr tx);
    void notify(size_t fork_height,
        system::header_const_ptr_list_const_ptr incoming,
        system::header_const_ptr_list_const_ptr outgoing);
    void notify(size_t fork_height,
        system::block_const_ptr_list_const_ptr incoming,
        system::block_const_ptr_list_const_ptr outgoing);

private:
    // Properties.
    system::uint256_t candidate_work() const;
    system::uint256_t confirmed_work() const;

    bool set_fork_point();
    bool set_candidate_work();
    bool set_confirmed_work();
    bool set_top_candidate_state();
    bool set_top_valid_candidate_state();
    bool set_next_confirmed_state();

    void set_fork_point(const system::config::checkpoint& fork);
    void set_candidate_work(const system::uint256_t& work_above_fork);
    void set_confirmed_work(const system::uint256_t& work_above_fork);
    void set_top_candidate_state(system::chain::chain_state::ptr top);
    void set_top_valid_candidate_state(system::chain::chain_state::ptr top);
    void set_next_confirmed_state(system::chain::chain_state::ptr top);

    // Utilities.
    void catalog_block(system::block_const_ptr block);
    void catalog_transaction(system::transaction_const_ptr tx);
    bool get_transactions(system::chain::transaction::list& out_transactions,
        const database::block_result& result, bool witness) const;
    bool get_transaction_hashes(system::hash_list& out_hashes,
        const database::block_result& result) const;

    // This is protected by mutex.
    database::data_base database_;

    // These are thread safe.
    std::atomic<bool> stopped_;

    system::atomic<system::config::checkpoint> fork_point_;
    system::atomic<system::uint256_t> candidate_work_;
    system::atomic<system::uint256_t> confirmed_work_;
    system::atomic<system::block_const_ptr> last_confirmed_block_;
    system::atomic<system::transaction_const_ptr> last_pool_transaction_;
    system::atomic<system::chain::chain_state::ptr> top_candidate_state_;
    system::atomic<system::chain::chain_state::ptr> top_valid_candidate_state_;
    system::atomic<system::chain::chain_state::ptr> next_confirmed_state_;

    const settings& settings_;
    const system::settings& bitcoin_settings_;
    const populate_chain_state chain_state_populator_;

    mutable system::upgrade_mutex candidate_mutex_;
    mutable system::prioritized_mutex confirmation_mutex_;
    mutable system::threadpool priority_pool_;
    mutable system::dispatcher priority_dispatch_;

    // The block pool is strictly a cache, so mutable.
    header_pool header_pool_;
    mutable block_pool block_pool_;
    transaction_pool transaction_pool_;

    organize_header organize_header_;
    organize_block organize_block_;
    organize_transaction organize_transaction_;

    block_subscriber::ptr block_subscriber_;
    header_subscriber::ptr header_subscriber_;
    transaction_subscriber::ptr transaction_subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
