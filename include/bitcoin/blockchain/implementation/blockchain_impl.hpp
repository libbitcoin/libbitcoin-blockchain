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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCKCHAIN_IMPL_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCKCHAIN_IMPL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoint.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/db_interface.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/implementation/organizer_impl.hpp>
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API blockchain_impl
  : public blockchain
{
public:

    blockchain_impl(threadpool& pool, const std::string& prefix,
        const db_active_heights& active_heights = { 0 },
        size_t orphan_capacity=20, bool testnet=false,
        const config::checkpoint::list& checks=checkpoint::mainnet);
    ~blockchain_impl();

    // Non-copyable
    blockchain_impl(const blockchain_impl&) = delete;
    void operator=(const blockchain_impl&) = delete;

    bool start();
    bool stop();

    void store(const chain::block& block, store_block_handler handle_store);
    void import(const chain::block& block, import_block_handler handle_import);

    // fetch block header by height
    void fetch_block_header(uint64_t height,
        fetch_handler_block_header handle_fetch);

    // fetch block header by hash
    void fetch_block_header(const hash_digest& hash,
        fetch_handler_block_header handle_fetch);

    // fetch transaction hashes in block by hash
    void fetch_block_transaction_hashes(const hash_digest& hash,
        fetch_handler_block_transaction_hashes handle_fetch);

    // fetch height of block by hash
    void fetch_block_height(const hash_digest& hash,
        fetch_handler_block_height handle_fetch);

    // fetch height of latest block
    void fetch_last_height(fetch_handler_last_height handle_fetch);

    // fetch transaction by hash
    void fetch_transaction(const hash_digest& hash,
        fetch_handler_transaction handle_fetch);

    // fetch height and offset within block of transaction by hash
    void fetch_transaction_index(const hash_digest& hash,
        fetch_handler_transaction_index handle_fetch);

    // fetch spend of an output point
    void fetch_spend(const chain::output_point& outpoint,
        fetch_handler_spend handle_fetch);

    // fetch outputs, values and spends for an address.
    void fetch_history(const wallet::payment_address& address,
        fetch_handler_history handle_fetch,
        const uint64_t limit=0, const uint64_t from_height=0);

    // fetch stealth results.
    void fetch_stealth(const binary_type& prefix,
        fetch_handler_stealth handle_fetch, uint64_t from_height=0);

    void subscribe_reorganize(reorganize_handler handle_reorganize);

private:
    typedef std::atomic<size_t> sequential_lock;
    typedef std::function<bool(uint64_t)> perform_read_functor;

    void initialize_lock(const std::string& prefix);
    void start_write();

    template <typename Handler, typename... Args>
    void stop_write(Handler handler, Args&&... args)
    {
        ++seqlock_;

        // seqlock is now even again.
        BITCOIN_ASSERT(seqlock_ % 2 == 0);
        handler(std::forward<Args>(args)...);
    }

    void do_store(const chain::block& block, store_block_handler handle_store);

    // Uses sequential lock to try to read shared data.
    // Try to initiate asynchronous read operation. If it fails then
    // sleep for a small amount of time and then retry read operation.
    void fetch(perform_read_functor perform_read);

    template <typename Handler, typename... Args>
    bool finish_fetch(uint64_t slock, Handler handler, Args&&... args)
    {
        if (slock != seqlock_)
            return false;

        handler(std::forward<Args>(args)...);
        return true;
    }

    bool do_fetch_stealth(const binary_type& prefix,
        fetch_handler_stealth handle_fetch, uint64_t from_height,
        uint64_t slock);

    bool stopped();

    // Queue for writes to the blockchain.
    dispatcher dispatch_;

    // Lock the database directory with a file lock.
    boost::interprocess::file_lock flock_;

    // sequential lock used for writes.
    sequential_lock seqlock_;
    bool stopped_;

    // Main database core.
    db_paths db_paths_;
    db_interface interface_;

    // Organize stuff
    orphans_pool orphans_;
    simple_chain_impl chain_;
    organizer_impl organizer_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif

