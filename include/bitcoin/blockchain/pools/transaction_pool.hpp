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
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/pools/transaction_pool_index.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API transaction_pool
{
public:
    typedef handle0 result_handler;

    // Don't use const reference for smart pointer parameters.
    typedef std::function<void(const code&, transaction_const_ptr)>
        fetch_handler;
    typedef std::function<void(const code&, inventory_ptr)>
        fetch_inventory_handler;
    typedef std::function<bool(const code&, transaction_const_ptr)>
        transaction_handler;

    typedef resubscriber<code, transaction_const_ptr>
        transaction_subscriber;

    static bool is_spent_by_tx(const chain::output_point& outpoint,
        transaction_const_ptr tx);

    /// Construct a transaction memory pool.
    transaction_pool(threadpool& pool, safe_chain& chain,
        const settings& settings);

    /// This class is not copyable.
    transaction_pool(const transaction_pool&) = delete;
    void operator=(const transaction_pool&) = delete;

    void start();
    void stop();

    void fetch_inventory(size_t limit, fetch_inventory_handler handler) const;
    void fetch(const hash_digest& tx_hash, fetch_handler handler) const;
    void fetch_history(const wallet::payment_address& address, size_t limit,
        size_t from_height, safe_chain::history_fetch_handler handler) const;
    void exists(const hash_digest& tx_hash, result_handler handler) const;
    void filter(get_data_ptr message, result_handler handler) const;
    void validate(transaction_const_ptr tx, result_handler handler) const;
    void organize(transaction_const_ptr tx, result_handler confirm_handler,
        result_handler validate_handler);

    /// Subscribe to transaction acceptance into the mempool.
    void subscribe_transaction(transaction_handler&& handler);

protected:
    typedef std::function<bool(const chain::input&)> input_compare;
    typedef boost::circular_buffer<transaction_const_ptr> buffer;
    typedef buffer::const_iterator const_iterator;

    bool stopped() const;
    const_iterator find_iterator(const hash_digest& tx_hash) const;

    bool handle_reorganized(const code& ec, size_t branch_point,
        block_const_ptr_list_const_ptr new_blocks,
        block_const_ptr_list_const_ptr replaced_blocks);
    void handle_validated(const code& ec, transaction_const_ptr tx,
        result_handler handler) const;

    void do_fetch_inventory(size_t limit,
        fetch_inventory_handler handler) const;
    void do_validate(transaction_const_ptr tx, result_handler handler) const;
    void do_organize(const code& ec, transaction_const_ptr tx,
        result_handler handle_confirm, result_handler handle_validate);

    void notify_transaction(transaction_const_ptr tx);

    void add(transaction_const_ptr tx, result_handler handler);
    void remove(block_const_ptr_list_const_ptr blocks);
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

    // This is thread safe.
    std::atomic<bool> stopped_;

    // This is protected by exclusive dispatch.
    buffer buffer_;

private:
    // These are not thread safe.
    bool is_in_pool(const hash_digest& tx_hash) const;
    bool is_spent_in_pool(transaction_const_ptr tx) const;
    bool is_spent_in_pool(const chain::output_point& outpoint) const;
    transaction_const_ptr find(const hash_digest& tx_hash) const;

    // These are thread safe.
    safe_chain& safe_chain_;
    transaction_pool_index index_;
    ////validate_transaction validator_;
    transaction_subscriber::ptr subscriber_;
    mutable dispatcher dispatch_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
