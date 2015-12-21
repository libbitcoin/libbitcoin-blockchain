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

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <system_error>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;

block_fetcher::block_fetcher(block_chain& chain)
  : blockchain_(chain)
{
}

void block_fetcher::fetch(block_chain& chain, uint64_t height,
    handler handle_fetch)
{
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(height, handle_fetch);
}

void block_fetcher::fetch(block_chain& chain, const hash_digest& hash,
    handler handle_fetch)
{
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(hash, handle_fetch);
}

void block_fetcher::handle_fetch_header(const std::error_code& ec,
    const chain::header& header, block_ptr block, handler handle_fetch)
{
    if (ec)
    {
        handle_fetch(ec, nullptr);
        return;
    }

    // Set the block header.
    block->header = header;

    blockchain_.fetch_block_transaction_hashes(header.hash(),
        std::bind(&block_fetcher::fetch_transactions,
            shared_from_this(), _1, _2, block, handle_fetch));
}

void block_fetcher::fetch_transactions(const std::error_code& ec,
    const hash_list& hashes, block_ptr block, handler handle_fetch)
{
    if (ec)
    {
        handle_fetch(ec, nullptr);
        return;
    }

    // Set the block transaction size.
    const auto count = hashes.size();
    block->transactions.resize(count);

    // This will be called exactly once by the synchronizer.
    const auto completion_handler =
        std::bind(&block_fetcher::handle_complete,
            shared_from_this(), _1, _2, handle_fetch);

    // Synchronize transaction fetch calls to one completion call.
    const auto complete = synchronizer<handler>(completion_handler, count,
        "block_fetcher");

    // blockchain::fetch_transaction is thread safe.
    size_t index = 0;
    for (const auto& hash: hashes)
        blockchain_.fetch_transaction(hash,
            std::bind(&block_fetcher::handle_fetch_transaction,
                shared_from_this(), _1, _2, index++, block, complete));
}

void block_fetcher::handle_fetch_transaction(const std::error_code& ec,
    const chain::transaction& transaction, size_t index, block_ptr block,
    handler handle_fetch)
{
    if (ec)
    {
        handle_fetch(ec, nullptr);
        return;
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////
    // A vector write cannot be executed concurrently with read|write.
    if (true)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Set a transaction into the block.
        block->transactions[index] = transaction;
    }
    ///////////////////////////////////////////////////////////////////////

    handle_fetch(error::success, block);
}

// If ec success then there is no possibility that block is being written.
void block_fetcher::handle_complete(const std::error_code& ec,
    block_ptr block, handler completion_handler)
{
    if (ec)
        completion_handler(ec, nullptr);
    else
        completion_handler(error::success, block);
}

} // namespace blockchain
} // namespace libbitcoin
