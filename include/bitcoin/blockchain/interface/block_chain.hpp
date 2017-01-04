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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/pools/block_organizer.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// The fast_chain interface portion of this class is not thread safe.
class BCB_API block_chain
  : public safe_chain, public fast_chain, noncopyable
{
public:
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
    // Thread safe except during write, unprotected by sequential lock.

    /// Get the set of block gaps in the chain.
    bool get_gaps(database::block_database::heights& out_gaps) const;

    /// Get a determination of whether the block hash exists in the store.
    bool get_block_exists(const hash_digest& block_hash) const;

    /// Get the hash of the block if it exists.
    bool get_block_hash(hash_digest& out_hash, size_t height) const;

    /// Get the difficulty of the branch starting at the given height.
    bool get_branch_difficulty(uint256_t& out_difficulty,
        const uint256_t& maximum, size_t height) const;

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
        size_t branch_height) const;

    /// Determine if an unspent transaction exists with the given hash.
    bool get_is_unspent_transaction(const hash_digest& hash,
        size_t branch_height) const;

    /// Get the transaction of the given hash and its block height.
    transaction_ptr get_transaction(size_t& out_block_height,
        const hash_digest& hash) const;

    // Synchronous writer.
    // ------------------------------------------------------------------------
    // Safe for concurrent execution with self (only).

    /// Set the flush lock scope (for use only with insert).
    bool begin_writes();

    /// Reset the flush lock scope (for use only with insert).
    bool end_writes();

    /// Insert a block to the blockchain, height is checked for existence.
    bool insert(block_const_ptr block, size_t height, bool flush);

    // Asynchronous writer.
    // ------------------------------------------------------------------------
    // Safe for concurrent execution with safe_chain reads.
    // Not safe for concurrent execution with other writes.

    /// Swap incoming and outgoing blocks, height is validated.
    void reorganize(branch::const_ptr branch,
        block_const_ptr_list_ptr outgoing_blocks, bool flush,
        dispatcher& dispatch, complete_handler handler);

    // ========================================================================
    // SAFE CHAIN
    // ========================================================================

    // Startup and shutdown.
    // ------------------------------------------------------------------------
    // Thread safe except start.

    /// Start or restart the blockchain.
    virtual bool start();

    /// Start the orphan pool and the transaction pool.
    virtual bool start_pools();

    /// Optional signal work stop, speeds shutdown with multiple threads.
    virtual bool stop();

    /// Unmaps all memory and frees the database file handles.
    /// Threads must be joined before close is called (or by destruct).
    virtual bool close();

    // Queries.
    // ------------------------------------------------------------------------
    // Thread safe.

    /// fetch a block by height.
    virtual void fetch_block(uint64_t height,
        block_fetch_handler handler) const;

    /// fetch a block by hash.
    virtual void fetch_block(const hash_digest& hash,
        block_fetch_handler handler) const;

    /// fetch block header by height.
    virtual void fetch_block_header(uint64_t height,
        block_header_fetch_handler handler) const;

    /// fetch block header by hash.
    virtual void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler) const;

    /// fetch hashes of transactions for a block, by block height.
    virtual void fetch_merkle_block(uint64_t height,
        transaction_hashes_fetch_handler handler) const;

    /// fetch hashes of transactions for a block, by block hash.
    virtual void fetch_merkle_block(const hash_digest& hash,
        transaction_hashes_fetch_handler handler) const;

    /// fetch height of block by hash.
    virtual void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler) const;

    /// fetch height of latest block.
    virtual void fetch_last_height(last_height_fetch_handler handler) const;

    /// fetch transaction by hash.
    virtual void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handler) const;

    /// fetch position and height within block of transaction by hash.
    virtual void fetch_transaction_position(const hash_digest& hash,
        transaction_index_fetch_handler handler) const;

    /// fetch the output of an outpoint (spent or otherwise).
    virtual void fetch_output(const chain::output_point& outpoint,
        output_fetch_handler handler) const;

    /// fetch the inpoint (spender) of an outpoint.
    virtual void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler) const;

    /// fetch outputs, values and spends for an address.
    virtual void fetch_history(const wallet::payment_address& address,
        uint64_t limit, uint64_t from_height,
        history_fetch_handler handler) const;

    /// fetch stealth results.
    virtual void fetch_stealth(const binary& filter, uint64_t from_height,
        stealth_fetch_handler handler) const;

    /// fetch a block locator relative to the current top and threshold.
    virtual void fetch_block_locator(const chain::block::indexes& heights,
        block_locator_fetch_handler handler) const;

    /// fetch the set of block hashes indicated by the block locator.
    virtual void fetch_locator_block_hashes(get_blocks_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        inventory_fetch_handler handler) const;

    /// fetch the set of block headers indicated by the block locator.
    virtual void fetch_locator_block_headers(get_headers_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const;

    // Transaction Pool.
    //-------------------------------------------------------------------------

    virtual void fetch_floaters(size_t limit,
        inventory_fetch_handler handler) const;

    // Filters.
    //-------------------------------------------------------------------------

    /// filter out block hashes that exist in the store.
    virtual void filter_blocks(get_data_ptr message,
        result_handler handler) const;

    /// filter out transaction hashes that exist in the store.
    virtual void filter_transactions(get_data_ptr message,
        result_handler handler) const;

    /// filter out block hashes that exist in the orphan pool.
    virtual void filter_orphans(get_data_ptr message,
        result_handler handler) const;

    /// filter out transaction hashes that exist in the store.
    virtual void filter_floaters(get_data_ptr message,
        result_handler handler) const;

    // Subscribers.
    //-------------------------------------------------------------------------

    /// Subscribe to blockchain reorganizations, get branch/height.
    virtual void subscribe_reorganize(reorganize_handler&& handler);

    /// Subscribe to memory pool additions, get tx and unconfirmed outputs.
    virtual void subscribe_transaction(transaction_handler&& handler);

    // Organizers (pools).
    //-------------------------------------------------------------------------

    /// Store a block in the orphan pool, triggers may trigger reorganization.
    virtual void organize(block_const_ptr block, result_handler handler);

    /// Store a transaction to the pool.
    virtual void organize(transaction_const_ptr transaction,
        transaction_store_handler handler);

    // Properties.
    //-----------------------------------------------------------------------------

    /// Get a reference to the blockchain configuration settings.
    const settings& chain_settings() const;

protected:

    /// Determine if work should terminate early with service stopped code.
    bool stopped() const;

private:
    typedef database::data_base::handle handle;

    // Sequential locking helpers.
    // ----------------------------------------------------------------------------

    template <typename Reader>
    void read_serial(const Reader& reader) const;

    template <typename Handler, typename... Args>
    bool finish_read(handle sequence, Handler handler, Args... args) const;

    // Utilities.
    //-----------------------------------------------------------------------------

    static hash_list to_hashes(const database::block_result& result);

    bool do_insert(const chain::block& block, size_t height);

    void handle_push(const code& ec, bool flush, result_handler handler);
    void handle_pop(const code& ec, branch::const_ptr branch, bool flush,
        dispatcher& dispatch, result_handler handler);

    // These are thread safe.
    std::atomic<bool> stopped_;
    const settings& settings_;
    asio::duration spin_lock_sleep_;
    block_pool block_pool_;
    block_organizer block_organizer_;
    transaction_pool transaction_pool_;
    ////transaction_organizer transaction_organizer_;
    database::data_base database_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
