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
#include <bitcoin/blockchain/pools/transaction_pool.hpp>

#include <cstddef>
#include <memory>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

// Duplicate tx hashes are disallowed in a block and therefore same in pool.
// A transaction hash that exists unspent in the chain is still not acceptable
// even if the original becomes spent in the same block, because the BIP30
// exmaple implementation simply tests all txs in a new block against
// transactions in previous blocks.

transaction_pool::transaction_pool(const settings& settings)
  ////: reject_conflicts_(settings.reject_conflicts),
  ////  minimum_fee_(settings.minimum_fee_satoshis)
{
}

// TODO: implement block template discovery.
void transaction_pool::fetch_template(merkle_block_fetch_handler handler) const
{
    const size_t height = max_size_t;
    const auto block = std::make_shared<message::merkle_block>();
    handler(error::success, block, height);
}

// TODO: implement mempool message payload discovery.
void transaction_pool::fetch_mempool(size_t maximum,
    inventory_fetch_handler handler) const
{
    const auto empty = std::make_shared<message::inventory>();
    handler(error::success, empty);
}

} // namespace blockchain
} // namespace libbitcoin
