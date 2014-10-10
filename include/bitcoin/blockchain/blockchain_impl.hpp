/*
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/db_interface.hpp>

namespace libbitcoin {
    namespace chain {

class blockchain_impl
  : public blockchain
{
public:
    // Used by internal components so need public definition here
    typedef subscriber<
        const std::error_code&, size_t, const block_list&, const block_list&>
            reorganize_subscriber_type;

    BCB_API blockchain_impl(threadpool& pool, const std::string& prefix);
    BCB_API ~blockchain_impl();

    // Non-copyable
    blockchain_impl(const blockchain_impl&) = delete;
    void operator=(const blockchain_impl&) = delete;

    BCB_API bool start();
    BCB_API void stop();

    BCB_API void store(const block_type& block,
        store_block_handler handle_store);
    BCB_API void import(const block_type& block,
        import_block_handler handle_import);

    // fetch block header by height
    BCB_API void fetch_block_header(size_t height,
        fetch_handler_block_header handle_fetch);
    // fetch block header by hash
    BCB_API void fetch_block_header(const hash_digest& hash,
        fetch_handler_block_header handle_fetch);
    // fetch transaction hashes in block by hash
    BCB_API void fetch_block_transaction_hashes(const hash_digest& hash,
        fetch_handler_block_transaction_hashes handle_fetch);
    // fetch height of block by hash
    BCB_API void fetch_block_height(const hash_digest& hash,
        fetch_handler_block_height handle_fetch);
    // fetch height of latest block
    BCB_API void fetch_last_height(fetch_handler_last_height handle_fetch);
    // fetch transaction by hash
    BCB_API void fetch_transaction(const hash_digest& hash,
        fetch_handler_transaction handle_fetch);
    // fetch height and offset within block of transaction by hash
    BCB_API void fetch_transaction_index(const hash_digest& hash,
        fetch_handler_transaction_index handle_fetch);
    // fetch spend of an output point
    BCB_API void fetch_spend(const output_point& outpoint,
        fetch_handler_spend handle_fetch);
    // fetch outputs, values and spends for an address.
    BCB_API void fetch_history(const payment_address& address,
        fetch_handler_history handle_fetch, size_t from_height=0);
    // fetch stealth results.
    BCB_API void fetch_stealth(const stealth_prefix& prefix,
        fetch_handler_stealth handle_fetch, size_t from_height=0);

    BCB_API void subscribe_reorganize(reorganize_handler handle_reorganize);

private:
    typedef std::atomic<size_t> seqlock_type;

    typedef std::function<bool (size_t)> perform_read_functor;

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

    void do_store(const block_type& block,
        store_block_handler handle_store);

    // Uses sequence looks to try to read shared data.
    // Try to initiate asynchronous read operation. If it fails then
    // sleep for a small amount of time and then retry read operation.
    void fetch(perform_read_functor perform_read);

    template <typename Handler, typename... Args>
    bool finish_fetch(size_t slock, Handler handler, Args&&... args)
    {
        if (slock != seqlock_)
            return false;
        handler(std::forward<Args>(args)...);
        return true;
    }

    bool do_fetch_stealth(const stealth_prefix& prefix,
        fetch_handler_stealth handle_fetch, size_t from_height, size_t slock);

    boost::asio::io_service& ios_;
    // Queue for writes to the blockchain.
    async_strand strand_;
    // Queue for serializing reorganization handler calls.
    async_strand reorg_strand_;

    // Lock the database directory with a file lock.
    boost::interprocess::file_lock flock_;
    // seqlock used for writes.
    seqlock_type seqlock_;

    // Main database core.
    db_interface interface_;

    // Organize stuff
    orphans_pool_ptr orphans_;
    simple_chain_ptr chain_;
    organizer_ptr organize_;

    reorganize_subscriber_type::ptr reorganize_subscriber_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

