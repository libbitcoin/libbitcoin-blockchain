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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_HPP

#include <atomic>
#include <cstddef>
#include <functional>
#include <boost/circular_buffer.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/transaction_pool_index.hpp>

namespace libbitcoin {
namespace blockchain {

/**
 * Before bitcoin transactions make it into a block, they go into
 * a transaction memory pool. This class encapsulates that functionality
 * performing the neccessary validation of a transaction before accepting
 * it into its internal buffer.
 *
 * The interface has been deliberately kept simple to minimise overhead.
 * This class attempts no tracking of inputs or spends and only provides
 * a store/fetch paradigm. Tracking must be performed externally and make
 * use of store's handle_store and handle_confirm to manage changes in the
 * state of memory pool transactions.
 */
class BCB_API transaction_pool
{
public:
    typedef handle0 exists_handler;
    typedef handle1<hash_list> missing_hashes_fetch_handler;
    typedef handle1<chain::transaction> fetch_handler;
    typedef handle2<chain::transaction, hash_digest> confirm_handler;
    typedef handle3<chain::transaction, hash_digest, chain::point::indexes>
        validate_handler;
    typedef std::function<bool(const code&, const chain::point::indexes&,
        const chain::transaction&)> transaction_handler;
    typedef resubscriber<const code&, const chain::point::indexes&,
        const chain::transaction&> transaction_subscriber;

    static bool is_spent_by_tx(const chain::output_point& outpoint,
        const chain::transaction& tx);

    /// Construct a transaction memory pool.
    transaction_pool(threadpool& pool, block_chain& chain,
        const settings& settings);

    /// Clear the pool, threads must be joined.
    ~transaction_pool();

    /// This class is not copyable.
    transaction_pool(const transaction_pool&) = delete;
    void operator=(const transaction_pool&) = delete;

    /// Start the threadpool (allow work to proceed).
    void start();

    /// Signal stop of current work, speeds shutdown, threads must be joined.
    void stop();

    void fetch(const hash_digest& tx_hash, fetch_handler handler);
    void fetch_history(const wallet::payment_address& address, size_t limit,
        size_t from_height, block_chain::history_fetch_handler handler);
    void fetch_missing_hashes(const hash_list& hashes,
        missing_hashes_fetch_handler handler);
    void exists(const hash_digest& tx_hash, exists_handler handler);
    void validate(const chain::transaction& tx, validate_handler handler);
    void store(const chain::transaction& tx, confirm_handler confirm_handler,
        validate_handler validate_handler);

    /// Subscribe to transaction acceptance into the mempool.
    void subscribe_transaction(transaction_handler handler);

    // TODO: these should be access-limited to validate_transaction.
    // These are not stranded so therefore are otherwise a thread safety issue.
    bool is_in_pool(const hash_digest& tx_hash) const;
    bool is_spent_in_pool(const chain::transaction& tx) const;
    bool is_spent_in_pool(const chain::output_point& outpoint) const;
    bool find(chain::transaction& out_tx, const hash_digest& tx_hash) const;

protected:
    struct entry
    {
        hash_digest hash;
        chain::transaction tx;
        confirm_handler handle_confirm;
    };

    typedef boost::circular_buffer<entry> buffer;
    typedef buffer::const_iterator iterator;
    typedef std::function<bool(const chain::input&)> input_compare;

    bool stopped();
    iterator find(const hash_digest& tx_hash) const;

    bool handle_reorganized(const code& ec, size_t fork_point,
        const chain::block::ptr_list& new_blocks,
        const chain::block::ptr_list& replaced_blocks);
    void handle_validated(const code& ec, const chain::transaction& tx,
        const hash_digest& hash, const chain::point::indexes& unconfirmed,
        validate_handler handler);

    void do_validate(const chain::transaction& tx, validate_handler handler);
    void do_store(const code& ec, const chain::transaction& tx,
        const hash_digest& hash, const chain::point::indexes& unconfirmed,
        confirm_handler handle_confirm, validate_handler handle_validate);

    void notify_transaction(const chain::point::indexes& unconfirmed,
        const chain::transaction& tx);

    void add(const chain::transaction& tx, confirm_handler handler);
    void remove(const chain::block::ptr_list& blocks);
    void clear(const code& ec);

    // testable private
    void delete_spent_in_blocks(const chain::block::ptr_list& blocks);
    void delete_confirmed_in_blocks(const chain::block::ptr_list& blocks);
    void delete_dependencies(const hash_digest& tx_hash, const code& ec);
    void delete_dependencies(const chain::output_point& point, const code& ec);
    void delete_dependencies(input_compare is_dependency, const code& ec);

    void delete_package(const code& ec);
    void delete_package(const chain::transaction& tx, const hash_digest& tx_hash,
        const code& ec);

    bool delete_single(const chain::transaction& tx, const code& ec);
    bool delete_single(const chain::transaction& tx, const hash_digest& tx_hash,
        const code& ec);

    std::atomic<bool> stopped_;
    const bool maintain_consistency_;

    // These are thread safe.
    buffer buffer_;
    dispatcher dispatch_;
    block_chain& blockchain_;
    transaction_pool_index index_;
    transaction_subscriber::ptr subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
