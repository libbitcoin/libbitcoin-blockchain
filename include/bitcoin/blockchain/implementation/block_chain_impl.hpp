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
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block_chain.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database.hpp>
#include <bitcoin/blockchain/implementation/organizer_impl.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API block_chain_impl
  : public block_chain, public simple_chain
{
public:
    // TODO: create threadpool internally from config.
    block_chain_impl(threadpool& pool,
        const settings& settings=settings::mainnet);

    /// This class is not copyable.
    block_chain_impl(const block_chain_impl&) = delete;
    void operator=(const block_chain_impl&) = delete;

    void start(result_handler handler);
    void stop(result_handler handler);
    void stop();

    // ------------------------------------------------------------------------
    // simple_chain

    /// Get the dificulty of a block at the given height.
    hash_number get_difficulty(uint64_t height);

    /// Get the height of the given block.
    bool get_height(uint64_t& out_height, const hash_digest& block_hash);

    /// Append the block to the top of the chain.
    void push(block_detail::ptr block);

    /// Remove blocks at or above the given height, returning them in order.
    bool pop_from(block_detail::list& out_blocks, uint64_t height);

    // ------------------------------------------------------------------------
    // block_chain

    /// Store a block to the blockchain, with FULL validation and indexing.
    void store(chain::block::ptr block, block_store_handler handler);

    /// Import a block to the blockchain, with NO validation or indexing.
    void import(chain::block::ptr block, block_import_handler handler);

    /// fetch a block locator relative to the current top and threshold
    void fetch_block_locator(block_locator_fetch_handler handler);

    /// fetch the set of block hashes indicated by the block locator
    void fetch_locator_block_hashes(const message::get_blocks& locator,
        const hash_digest& threshold,
        locator_block_hashes_fetch_handler handler);

    /// fetch subset of specified block hashes that are not stored
    void fetch_missing_block_hashes(const hash_list& hashes,
        missing_block_hashes_fetch_handler handler);

    /// fetch block header by height
    void fetch_block_header(uint64_t height,
        block_header_fetch_handler handler);

    /// fetch block header by hash
    void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler);

    /// fetch hashes of transactions for a block, by block height
    void fetch_block_transaction_hashes(uint64_t height,
        transaction_hashes_fetch_handler handler);

    /// fetch hashes of transactions for a block, by block hash
    void fetch_block_transaction_hashes(const hash_digest& hash,
        transaction_hashes_fetch_handler handler);

    /// fetch height of block by hash
    void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler);

    /// fetch height of latest block
    void fetch_last_height(last_height_fetch_handler handler);

    /// fetch transaction by hash
    void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handler);

    /// fetch height and offset within block of transaction by hash
    void fetch_transaction_index(const hash_digest& hash,
        transaction_index_fetch_handler handler);

    /// fetch spend of an output point
    void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler);

    /// fetch outputs, values and spends for an address.
    void fetch_history(const wallet::payment_address& address,
        uint64_t limit, uint64_t from_height, history_fetch_handler handler);

    /// fetch stealth results.
    void fetch_stealth(const binary& filter, uint64_t from_height,
        stealth_fetch_handler handler);

    /// Subscribe to blockchain reorganizations.
    void subscribe_reorganize(organizer::reorganize_handler handler);

private:
    typedef std::atomic<size_t> sequential_lock;
    typedef std::function<bool(uint64_t)> perform_read_functor;

    void initialize_lock(const std::string& prefix);
    void start_write();

    template <typename Handler, typename... Args>
    void stop_write(Handler handler, Args&&... args)
    {
        ++slock_;

        // slock_ is now even again.
        BITCOIN_ASSERT(slock_ % 2 == 0);
        handler(std::forward<Args>(args)...);
    }

    void do_store(chain::block::ptr block, block_store_handler handler);
    void do_import(chain::block::ptr block, block_import_handler handler);

    // Fetch uses sequential lock to try to read shared data.
    // Try to initiate asynchronous read operation. If it fails then
    // sleep for a small amount of time and then retry read operation.

    // Asynchronous fetch.
    void fetch_parallel(perform_read_functor perform_read);

    // Ordered fetch (order only amoung other calls to fetch_ordered).
    void fetch_ordered(perform_read_functor perform_read);

    template <typename Handler, typename... Args>
    bool finish_fetch(uint64_t slock, Handler handler, Args&&... args)
    {
        if (slock != slock_)
            return false;

        handler(std::forward<Args>(args)...);
        return true;
    }

    bool stopped();

    // Queue for reads of the blockchain.
    dispatcher read_dispatch_;

    // Queue for writes to the blockchain.
    dispatcher write_dispatch_;

    // Lock the database directory with a file lock.
    boost::interprocess::file_lock flock_;

    // sequential lock used for writes.
    sequential_lock slock_;
    bool stopped_;

    // Main database core.
    database::store store_;
    database database_;

    // Organize stuff
    orphan_pool orphans_;
    organizer_impl organizer_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif

