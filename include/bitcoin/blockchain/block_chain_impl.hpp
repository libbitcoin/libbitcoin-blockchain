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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_IMPL_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_IMPL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/block_chain.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API block_chain_impl
  : public block_chain, public simple_chain
{
public:
    block_chain_impl(const blockchain::settings& 
        chain_settings=blockchain::settings::mainnet,
        const database::settings&
        database_settings=database::settings::mainnet);

    /// The thread pool is stopped on destruct.
    ~block_chain_impl();

    /// This class is not copyable.
    block_chain_impl(const block_chain_impl&) = delete;
    void operator=(const block_chain_impl&) = delete;

    // Properties.
    // ------------------------------------------------------------------------

    // Get a reference to the transaction pool.
    transaction_pool& transaction_pool();

    // Get a reference to the blockchain configuration settings.
    const settings& chain_settings() const;

    // block_chain start/stop (TODO: this also affects simple_chain).
    // ------------------------------------------------------------------------

    void start(result_handler handler);
    void stop(result_handler handler);
    void close();

    // simple_chain (no internal locks).
    // ------------------------------------------------------------------------

    /// Get the dificulty of a block at the given height.
    bool get_difficulty(hash_number& out_difficulty, uint64_t height);

    /// Get the header of the block at the given height.
    bool get_header(chain::header& out_header, uint64_t height);

    /// Get the height of the block with the given hash.
    bool get_height(uint64_t& out_height, const hash_digest& block_hash);

    /// Get height of latest block.
    bool get_last_height(size_t& out_height);

    /// Get the hash digest of the transaction of the outpoint.
    bool get_outpoint_transaction(hash_digest& out_transaction,
        const chain::output_point& outpoint);

    /// Get the transaction of the given hash and its block height.
    bool get_transaction(chain::transaction& out_transaction,
        uint64_t& out_block_height, const hash_digest& transaction_hash);

    /// Append the block to the top of the chain.
    bool push(block_detail::ptr block);

    /// Remove blocks at or above the given height, returning them in order.
    bool pop_from(block_detail::list& out_blocks, uint64_t height);

    // block_chain queries (internal locks).
    // ------------------------------------------------------------------------

    /// Import a block to the blockchain, with indexing and NO validation.
    bool import(chain::block::ptr block, uint64_t height);

    /// Store a block to the blockchain, with indexing and FULL validation.
    void store(chain::block::ptr block, block_store_handler handler);

    /// fetch a block locator relative to the current top and threshold.
    void fetch_block_locator(block_locator_fetch_handler handler);

    /// fetch the set of block hashes indicated by the block locator.
    void fetch_locator_block_hashes(const message::get_blocks& locator,
        const hash_digest& threshold, size_t limit,
        locator_block_hashes_fetch_handler handler);

    /// fetch subset of specified block hashes that are not stored.
    void fetch_missing_block_hashes(const hash_list& hashes,
        missing_block_hashes_fetch_handler handler);

    /// fetch block header by height.
    void fetch_block_header(uint64_t height,
        block_header_fetch_handler handler);

    /// fetch block header by hash.
    void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler);

    /// fetch hashes of transactions for a block, by block height.
    void fetch_block_transaction_hashes(uint64_t height,
        transaction_hashes_fetch_handler handler);

    /// fetch hashes of transactions for a block, by block hash.
    void fetch_block_transaction_hashes(const hash_digest& hash,
        transaction_hashes_fetch_handler handler);

    /// fetch height of block by hash.
    void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler);

    /// fetch height of latest block.
    void fetch_last_height(last_height_fetch_handler handler);

    /// fetch transaction by hash.
    void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handler);

    /// fetch height and offset within block of transaction by hash.
    void fetch_transaction_index(const hash_digest& hash,
        transaction_index_fetch_handler handler);

    /// fetch spend of an output point.
    void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler);

    /// fetch outputs, values and spends for an address.
    void fetch_history(const wallet::payment_address& address,
        uint64_t limit, uint64_t from_height, history_fetch_handler handler);

    /// fetch stealth results.
    void fetch_stealth(const binary& filter, uint64_t from_height,
        stealth_fetch_handler handler);

    /// Subscribe to blockchain reorganizations.
    void subscribe_reorganize(reorganize_handler handler);

private:
    typedef std::function<bool(database::handle)> perform_read_functor;

    template <typename Handler, typename... Args>
    bool finish_fetch(database::handle handle, Handler handler, Args&&... args)
    {
        if (!database_.is_read_valid(handle))
            return false;

        handler(std::forward<Args>(args)...);
        return true;
    }

    template <typename Handler, typename... Args>
    void stop_write(Handler handler, Args&&... args)
    {
        const auto result = database_.end_write();
        BITCOIN_ASSERT(result);
        handler(std::forward<Args>(args)...);
    }

    void start_write();
    void do_store(chain::block::ptr block, block_store_handler handler);
    void fetch_ordered(perform_read_functor perform_read);
    void fetch_parallel(perform_read_functor perform_read);
    void handle_stopped(const code&);
    bool stopped();

    std::atomic<bool> stopped_;
    const settings& settings_;
    mutable shared_mutex mutex_;

    threadpool threadpool_;
    organizer organizer_;
    dispatcher read_dispatch_;
    dispatcher write_dispatch_;
    database::data_base database_;
    blockchain::transaction_pool transaction_pool_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
