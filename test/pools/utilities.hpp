/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_TEST_POOLS_UTILITIES_HPP
#define LIBBITCOIN_BLOCKCHAIN_TEST_POOLS_UTILITIES_HPP

#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace blockchain {
namespace test {
namespace pools {

/// This class is not thread safe.
class utilities
{
public:
    static chain::chain_state::data get_chain_data();

    static transaction_const_ptr get_const_tx(uint32_t version,
        uint32_t locktime);

    static transaction_entry::ptr get_entry(chain::chain_state::ptr state,
        uint32_t version, uint32_t locktime);

    static transaction_entry::ptr get_fee_entry(chain::chain_state::ptr state,
        uint32_t version, uint32_t locktime, uint64_t fee);

    static void connect(transaction_entry::ptr parent,
        transaction_entry::ptr child, uint32_t index);

    static void sever(transaction_entry::ptr entry);

    static void sever(transaction_entry::list entries);

    static bool ordered_entries_equal(transaction_entry::list left,
        transaction_entry::list right);

    static bool unordered_entries_equal(transaction_entry::list left,
        transaction_entry::list right);
};

} // namespace pools
} // namespace test
} // namespace blockchain
} // namespace libbitcoin

#endif
