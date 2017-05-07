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
    // Thread safe, unprotected by sequential lock.

    /// Get the set of block gaps in the chain.
    bool get_gaps(database::block_database::heights& out_gaps) const;

    /// Get a determination of whether the block hash exists in the store.
    bool get_block_exists(const hash_digest& block_hash) const;

    /// Get the hash of the block if it exists.
    bool get_block_hash(hash_digest& out_hash, size_t height) const;

    /// Get the work of the branch starting at the given height.
    bool get_branch_work(uint256_t& out_work, const uint256_t& maximum,
        size_t height) const;

    /// Get the header of the block at the given height.
    bool get_header(chain::header& out_header, size_t height) const;

    /// Get the height of the block with the given hash.
    bool get_height(size_t& out_height, const hash_digest& block_hash) const;

    /// Get the bits of the block with the given height.
    bool get_bits(uint32_t& out_bits, const size_t& height) const;

    /// Get the timestamp of the block with the given height.
    bool get_timestamp(uint32_t& out_timestamp, const size_t& height) const;

    /// Get the version of the block with the given height.
    bool get_version(uint32_t& out_version, const size_t& height) const;

    /// Get height of latest block.
    bool get_last_height(size_t& out_height) const;

    /// Get the output that is referenced by the outpoint.
    bool get_output(chain::output& out_output, size_t& out_height,
        bool& out_coinbase, const chain::output_point& outpoint,
        size_t branch_height, bool require_confirmed) const;

    /// Determine if an unspent transaction exists with the given hash.
    bool get_is_unspent_transaction(const hash_digest& hash,
        size_t branch_height, bool require_confirmed) const;

    /// Get position data for a transaction.
    bool get_transaction_position(size_t& out_height, size_t& out_position,
        const hash_digest& hash, bool require_confirmed) const;

    /////// Get the transaction of the given hash and its block height.
    ////transaction_ptr get_transaction(size_t& out_block_height,
    ////    const hash_digest& hash, bool require_confirmed) const;

    // Writers.
    // ------------------------------------------------------------------------
    // Thread safe, insert does not set sequential lock.

    /// Create flush lock if flush_writes is true, and set sequential lock.
    bool begin_insert() const;

    /// Clear flush lock if flush_writes is true, and clear sequential lock.
    bool end_insert() const;

    /// Insert a block to the blockchain, height is checked for existence.
    /// Reads and reorgs are undefined when chain is gapped.
    bool insert(block_const_ptr block, size_t height);

    /// Push an unconfirmed transaction to the tx table and index outputs.
    void push(transaction_const_ptr tx, dispatcher& dispatch,
        result_handler handler);

    /// Swap incoming and outgoing blocks, height is validated.
    void reorganize(const config::checkpoint& fork_point,
        block_const_ptr_list_const_ptr incoming_blocks,
        block_const_ptr_list_ptr outgoing_blocks, dispatcher& dispatch,
        result_handler handler);

    // Properties
    // ------------------------------------------------------------------------

    /// Get chain state for transaction pool, relative to chain top.
    chain::chain_state::ptr chain_state() const;

    /// Get chain state for header, relative to header's parent.
    chain::chain_state::ptr chain_state(header_const_ptr header) const;

    /// Get full chain state relative to the branch top.
    chain::chain_state::ptr chain_state(branch::const_ptr branch) const;

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
    void fetch_block(size_t height, block_fetch_handler handler) const;

    /// fetch a block by hash.
    void fetch_block(const hash_digest& hash,
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
        transaction_fetch_handler handler) const;

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

    /// fetch an inventory locator relative to the current top and threshold.
    void fetch_block_locator(const chain::block::indexes& heights,
        block_locator_fetch_handler handler) const;

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

    /// Subscribe to blockchain reorganizations, get branch/height.
    void subscribe_blockchain(reorganize_handler&& handler);

    /// Subscribe to memory pool additions, get transaction.
    void subscribe_transaction(transaction_handler&& handler);

    /// Send null data success notification to all subscribers.
    void unsubscribe();

    // Organizers.
    //-------------------------------------------------------------------------

    /// Organize a block into the block pool if valid.
    void organize(block_const_ptr block, result_handler handler);

    /// Organize a header into the header pool if valid.
    void organize(header_const_ptr header, result_handler handler);

    /// Store a transaction to the pool if valid.
    void organize(transaction_const_ptr tx, result_handler handler);

    // Properties.
    //-------------------------------------------------------------------------

    /// True if the top block age exceeds the configured limit.
    bool is_stale() const;

    /// Get a reference to the blockchain configuration settings.
    const settings& chain_settings() const;

protected:

    /// Determine if work should terminate early with service stopped code.
    bool stopped() const;

private:
    // Utilities.
    //-------------------------------------------------------------------------

    void set_pool_state(const chain::chain_state& top);
    void handle_transaction(const code& ec, transaction_const_ptr tx,
        result_handler handler) const;
    void handle_block(const code& ec, block_const_ptr block,
        result_handler handler) const;
    void handle_reorganize(const code& ec, block_const_ptr top,
        result_handler handler);

    // These are thread safe.
    std::atomic<bool> stopped_;
    const settings& settings_;
    const time_t notify_limit_seconds_;
    bc::atomic<block_const_ptr> last_block_;
    bc::atomic<transaction_const_ptr> last_transaction_;
    const populate_chain_state chain_state_populator_;
    bc::atomic<chain::chain_state::ptr> pool_state_;
    database::data_base database_;

    // These are thread safe.
    mutable prioritized_mutex validation_mutex_;
    mutable threadpool priority_pool_;
    mutable dispatcher dispatch_;
    header_organizer header_organizer_;
    block_organizer block_organizer_;
    transaction_organizer transaction_organizer_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
