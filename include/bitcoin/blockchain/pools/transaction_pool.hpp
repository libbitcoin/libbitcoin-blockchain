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
#include <bitcoin/blockchain/interface/full_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/pools/transaction_pool_index.hpp>
#include <bitcoin/blockchain/validation/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API transaction_pool
{
public:
    typedef chain::point::indexes indexes;

    typedef handle0 result_handler;
    typedef handle1<indexes> validate_handler;

    // Don'ut use const reference for smart pointer parameters.
    typedef std::function<void(const code&, transaction_const_ptr)>
        fetch_handler;
    typedef std::function<bool(const code&, const indexes&,
        transaction_const_ptr)> transaction_handler;
    typedef resubscriber<const code&, const indexes&, transaction_const_ptr>
        transaction_subscriber;

    static bool is_spent_by_tx(const chain::output_point& outpoint,
        transaction_const_ptr tx);

    /// Construct a transaction memory pool.
    transaction_pool(threadpool& pool, full_chain& chain,
        const settings& settings);

    /// Clear the pool, threads must be joined.
    ~transaction_pool();

    /// This class is not copyable.
    transaction_pool(const transaction_pool&) = delete;
    void operator=(const transaction_pool&) = delete;

    /// Start the transaction pool.
    void start();

    /// Signal stop of current work, speeds shutdown.
    void stop();

    inventory_ptr fetch_inventory() const;
    void fetch(const hash_digest& tx_hash, fetch_handler handler) const;
    void fetch_history(const wallet::payment_address& address, size_t limit,
        size_t from_height, full_chain::history_fetch_handler handler) const;
    void exists(const hash_digest& tx_hash, result_handler handler) const;
    void filter(get_data_ptr message, result_handler handler) const;
    void validate(transaction_const_ptr tx, validate_handler handler) const;
    void store(transaction_const_ptr tx, result_handler confirm_handler,
        validate_handler validate_handler);

    /// Subscribe to transaction acceptance into the mempool.
    void subscribe_transaction(transaction_handler handler);

protected:
    typedef boost::circular_buffer<transaction_const_ptr> buffer;
    typedef buffer::const_iterator const_iterator;

    typedef std::function<bool(const chain::input&)> input_compare;

    bool stopped() const;
    const_iterator find_iterator(const hash_digest& tx_hash) const;

    bool handle_reorganized(const code& ec, size_t fork_point,
        const block_const_ptr_list& new_blocks,
        const block_const_ptr_list& replaced_blocks);
    void handle_validated(const code& ec, const indexes& unconfirmed,
        transaction_const_ptr tx, validate_transaction::ptr self,
        validate_handler handler) const;

    void do_validate(transaction_const_ptr tx, validate_handler handler) const;
    void do_store(const code& ec, const indexes& unconfirmed,
        transaction_const_ptr tx, result_handler handle_confirm,
        validate_handler handle_validate);

    void notify_transaction(const indexes& unconfirmed,
        transaction_const_ptr tx);

    void add(transaction_const_ptr tx, result_handler handler);
    void remove(const block_const_ptr_list& blocks);
    void clear(const code& ec);

    // These would be private but for test access.
    void delete_spent_in_blocks(const block_const_ptr_list& blocks);
    void delete_confirmed_in_blocks(const block_const_ptr_list& blocks);
    void delete_dependencies(const hash_digest& tx_hash, const code& ec);
    void delete_dependencies(const chain::output_point& point, const code& ec);
    void delete_dependencies(input_compare is_dependency, const code& ec);
    void delete_package(const code& ec);
    void delete_package(transaction_const_ptr tx, const code& ec);
    bool delete_single(const hash_digest& tx_hash, const code& ec);

    // The buffer is protected by non-concurrent dispatch.
    buffer buffer_;
    std::atomic<bool> stopped_;

private:
    ////// Unsafe methods limited to friend caller.
    ////friend class validate_transaction;

    // These methods are NOT thread safe.
    bool is_in_pool(const hash_digest& tx_hash) const;
    bool is_spent_in_pool(transaction_const_ptr tx) const;
    bool is_spent_in_pool(const chain::output_point& outpoint) const;
    transaction_const_ptr find(const hash_digest& tx_hash) const;

    // These are thread safe.
    full_chain& blockchain_;
    transaction_pool_index index_;
    transaction_subscriber::ptr subscriber_;
    mutable dispatcher dispatch_;
    const bool maintain_consistency_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
