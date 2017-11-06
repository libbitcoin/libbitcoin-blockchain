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
#ifndef LIBBITCOIN_BLOCKCHAIN_TEST_POOLS_UTILITIES_HPP
#define LIBBITCOIN_BLOCKCHAIN_TEST_POOLS_UTILITIES_HPP

#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace blockchain {
namespace test {
namespace pools {

/// This class is not thread safe.
class BCB_API utilities
{
public:
    static bc::chain::chain_state::data get_chain_data();

    static bc::transaction_const_ptr get_const_tx(uint32_t version,
        uint32_t locktime);

    static bc::blockchain::transaction_entry::ptr get_entry(
        bc::chain::chain_state::ptr state, uint32_t version,
        uint32_t locktime);

    static bc::blockchain::transaction_entry::ptr get_fee_entry(
        bc::chain::chain_state::ptr state, uint32_t version,
        uint32_t locktime, uint64_t fee);

    static void connect(bc::blockchain::transaction_entry::ptr parent,
        bc::blockchain::transaction_entry::ptr child, uint32_t index);

    static void sever(bc::blockchain::transaction_entry::ptr entry);

    static void sever(bc::blockchain::transaction_entry::list entries);

    static bool ordered_entries_equal(
        bc::blockchain::transaction_entry::list alpha,
        bc::blockchain::transaction_entry::list beta);

    static bool unordered_entries_equal(
        bc::blockchain::transaction_entry::list alpha,
        bc::blockchain::transaction_entry::list beta);
};

} // namespace pools
} // namespace test
} // namespace blockchain
} // namespace libbitcoin

#endif
