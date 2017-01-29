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

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API transaction_pool
{
public:
    typedef safe_chain::inventory_fetch_handler mempool_fetch_handler;

    transaction_pool(const settings& settings);

    void fetch_template(mempool_fetch_handler) const;
    void fetch_mempool(size_t maximum, mempool_fetch_handler) const;

private:
    const bool reject_conflicts_;
    const uint64_t minimum_fee_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
