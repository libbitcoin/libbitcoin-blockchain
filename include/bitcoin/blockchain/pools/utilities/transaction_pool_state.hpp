/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_STATE_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_STATE_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace libbitcoin {
namespace blockchain {

struct BCB_API transaction_pool_state
{
public:
    typedef double priority;

    // A bidirection map is used for efficient output and position retrieval.
    // This produces the effect of a prioritized buffer tx hash table of outputs.
    // TODO: replace with multi_indexed_container tracking dependency ordering
    // for mempool/template emission efficiency
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<transaction_entry::ptr,
            boost::hash<boost::bimaps::tags::support::value_type_of<
                transaction_entry::ptr>::type>,
            transaction_entry::ptr_equal>,
        boost::bimaps::multiset_of<priority, std::greater<priority>>> prioritized_transactions;

    transaction_pool_state();

    transaction_pool_state(const settings& settings);

    ~transaction_pool_state();

    size_t block_template_bytes;
    size_t block_template_sigops;
    prioritized_transactions block_template;
    prioritized_transactions pool;

    size_t template_byte_limit;
    size_t template_sigop_limit;
    size_t coinbase_byte_reserve;
    size_t coinbase_sigop_reserve;

    std::map<transaction_entry::ptr, transaction_entry::list> cached_child_closures;
    transaction_entry::list ordered_block_template;

private:
    void disconnect_entries();
};

} // namespace blockchain
} // namespace libbitcoin

#endif
