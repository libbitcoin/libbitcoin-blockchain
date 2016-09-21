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
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/full_chain.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>

namespace libbitcoin {
namespace blockchain {

/// The simple_chain interface portion of this class is not thread safe.
class BCB_API block_chain
  : public full_chain, public simple_chain
{
public:
    block_chain(threadpool& pool, 
        const blockchain::settings& chain_settings,
        const database::settings& database_settings);

    /// The database is closed on destruct, threads must be joined.
    ~block_chain();

    /// This class is not copyable.
    block_chain(const block_chain&) = delete;
    void operator=(const block_chain&) = delete;

    // Properties (thread safe).
    // ------------------------------------------------------------------------

    // Get a reference to the blockchain configuration settings.
    const settings& chain_settings() const;

    // full_chain start/stop (thread safe).
    // ------------------------------------------------------------------------

    /// Start or restart the blockchain.
    virtual bool start();

    /// Signal stop of current work, speeds shutdown with multiple threads.
    virtual bool stop();

    /// Close the blockchain, threads must first be joined, can be restarted.
    virtual bool close();

    // simple_chain getters (NOT THREAD SAFE).
    // ------------------------------------------------------------------------

    /// Return the first and last gaps in the blockchain, or false if none.
    bool get_gap_range(uint64_t& out_first, uint64_t& out_last) const;

    /// Return the next chain gap at or after the specified start height.
    bool get_next_gap(uint64_t& out_height, uint64_t start_height) const;

    /// Get the dificulty of a block at the given height.
    bool get_difficulty(hash_number& out_difficulty, uint64_t height) const;

    /// Get the header of the block at the given height.
    bool get_header(chain::header& out_header, uint64_t height) const;

    /// Get the height of the block with the given hash.
    bool get_height(uint64_t& out_height, const hash_digest& block_hash) const;

    /// Get height of latest block.
    bool get_last_height(uint64_t& out_height) const;

    /// Get the hash digest of the transaction of the outpoint.
    bool get_outpoint_transaction(hash_digest& out_hash,
        const chain::output_point& outpoint) const;

    /// Get the transaction of the given hash and its block height.
    transaction_ptr get_transaction(uint64_t& out_block_height,
        const hash_digest& transaction_hash) const;

    /// Get the block height of the transaction given its hash.
    bool get_transaction_height(uint64_t& out_block_height,
        const hash_digest& transaction_hash) const;

    // simple_chain setters (NOT THREAD SAFE).
    // ------------------------------------------------------------------------

    /// Import a block to the blockchain.
    bool import(block_const_ptr block, uint64_t height);

    /// Append the block to the top of the chain.
    bool push(block_const_ptr block);

    /// Remove blocks at or above the given height, returning them in order.
    bool pop_from(block_const_ptr_list& out_blocks, uint64_t height);

    // full_chain queries (thread safe).
    // ------------------------------------------------------------------------

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
        locator_block_hashes_fetch_handler handler) const;

    /// fetch the set of block headers indicated by the block locator.
    virtual void fetch_locator_block_headers(get_headers_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const;

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

    /// Subscribe to blockchain reorganizations, get forks/height.
    virtual void subscribe_reorganize(reorganize_handler handler);

    /// Subscribe to memory pool additions, get tx and unconfirmed outputs.
    virtual void subscribe_transaction(transaction_handler handler);

    // Stores.
    //-------------------------------------------------------------------------

    /// Store a block to the block chain (via orphan pool).
    virtual void store(block_const_ptr block, block_store_handler handler);

    /// Store a transaction to the memory pool.
    virtual void store(transaction_const_ptr transaction,
        transaction_store_handler handler);

private:
    typedef std::function<bool(database::handle)> perform_read_functor;

    template <typename Handler, typename... Args>
    bool finish_fetch(database::handle handle,
        Handler handler, Args&&... args) const
    {
        if (!database_.is_read_valid(handle))
            return false;

        handler(std::forward<Args>(args)...);
        return true;
    }

    void do_store(block_const_ptr block, block_store_handler handler);

    void fetch_serial(perform_read_functor perform_read) const;
    bool stopped() const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const settings& settings_;
    organizer organizer_;
    blockchain::transaction_pool transaction_pool_;

    // This is protected by mutex.
    database::data_base database_;
    mutable shared_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
