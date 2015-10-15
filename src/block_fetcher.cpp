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
#include <bitcoin/blockchain/block_fetcher.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <system_error>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;

void block_fetcher::fetch(block_chain& chain, uint64_t height,
    handler handle_fetch)
{
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(height, handle_fetch);
}

void block_fetcher::fetch(block_chain& chain, const hash_digest& block_hash,
    handler handle_fetch)
{
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(block_hash, handle_fetch);
}

block_fetcher::block_fetcher(block_chain& chain)
  : handler_(nullptr), blockchain_(chain), stopped_(false)
{
}


bool block_fetcher::stop_on_error(const code& ec)
{
    if (stopped_)
        return true;

    if (ec)
    {
        stopped_ = true;
        handler_(ec, chain::block());
        return true;
    }

    return false;
}

void block_fetcher::fetch_tx(const hash_digest& tx_hash, size_t tx_index)
{
    const auto handle_fetch = [this, tx_index](const code& ec,
        const chain::transaction& tx)
    {
        if (stop_on_error(ec))
            return;

        BITCOIN_ASSERT(tx_index < block_.transactions.size());
        block_.transactions[tx_index] = tx;

        // Atomicity: must increment and read value in one instruction if
        // transactions are retrieved concurrently (which is not yet the case).
        const auto handled_count = ++handled_count_;

        if (handled_count == block_.transactions.size())
            handler_(error::success, block_);
    };

    blockchain_.fetch_transaction(tx_hash, handle_fetch);
}

void block_fetcher::fetch_transactions(const code& ec,
    const hash_list& tx_hashes)
{
    if (stop_on_error(ec))
        return;

    ////////////////// TODO: use synchronizer //////////////////
    block_.transactions.resize(tx_hashes.size());
    handled_count_ = 0;
    for (size_t tx_index = 0; tx_index < tx_hashes.size(); ++tx_index)
        fetch_tx(tx_hashes[tx_index], tx_index);
}

////void block_fetcher::fetch_hashes()
////{
////    blockchain_.fetch_block_transaction_hashes(
////        hash_block_header(block_.header),
////            std::bind(&fetch_block_t::fetch_transactions,
////                shared_from_this(), _1, _2));
////}

} // namespace blockchain
} // namespace libbitcoin
