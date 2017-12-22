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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>
#include <bitcoin/blockchain/pools/transaction_pool_state.hpp>

namespace libbitcoin {
namespace blockchain {

/// TODO: this class is not implemented or utilized.
class BCB_API transaction_pool
{
public:
    typedef safe_chain::inventory_fetch_handler inventory_fetch_handler;
    typedef safe_chain::merkle_block_fetch_handler merkle_block_fetch_handler;
    typedef double priority;

    transaction_pool(const settings& settings);

    /// The tx exists in the pool.
    bool exists(transaction_const_ptr tx) const;

    /// Remove all message vectors that match transaction hashes.
    void filter(get_data_ptr message) const;

    void fetch_mempool(size_t maximum, inventory_fetch_handler) const;
    void fetch_template(merkle_block_fetch_handler) const;

    transaction_entry::list get_mempool() const;
    transaction_entry::list get_template() const;

    void add_unconfirmed_transactions(
        const transaction_const_ptr_list& unconfirmed_txs);

    void remove_transactions(transaction_const_ptr_list& txs);

private:
    typedef transaction_pool_state::prioritized_transactions::right_map::iterator
        priority_iterator;

    priority calculate_priority(transaction_entry::ptr tx);

    priority_iterator find_inflection(
        transaction_pool_state::prioritized_transactions& container,
        transaction_pool::priority value);

    transaction_entry::list get_parent_closure(transaction_entry::ptr tx);

    void populate_child_closure(transaction_entry::ptr tx);

    void order_template_transactions();

    void update_template(priority_iterator max_pool_change);

private:
    transaction_pool_state state_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
