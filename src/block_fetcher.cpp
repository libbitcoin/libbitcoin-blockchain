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
#include <utility>
#include <system_error>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace chain;
using namespace std::placeholders;

// TODO: split into header.
// This class is used only locally, builds block from header and transactions.
class block_fetcher
  : public std::enable_shared_from_this<block_fetcher>
{
public:
    block_fetcher(const block_chain& chain)
      : blockchain_(chain)
    {
    }

    template <typename BlockIndex>
    void start(const BlockIndex& index,
        block_chain::block_fetch_handler handler)
    {
        // block_ptr must be non-const.
        blockchain_.fetch_block_header(index,
            std::bind(&block_fetcher::handle_fetch_header,
                shared_from_this(), _1, _2, _3, handler));
    }

private:

    // header_ptr must be non-const.
    void handle_fetch_header(const code& ec, header_ptr header,
        uint64_t height, block_chain::block_fetch_handler handler)
    {
        if (ec)
        {
            handler(ec, nullptr, 0);
            return;
        }

        const auto txs = chain::transaction::list(header->transaction_count);

        // Create the block using the header and emplty transactions.
        const auto block = std::make_shared<message::block_message>(
            std::move(*header), std::move(txs));

        blockchain_.fetch_block_transaction_hashes(block->hash(),
            std::bind(&block_fetcher::fetch_transactions,
                shared_from_this(), _1, _2, block, height, handler));
    }

    // block_ptr must be non-const.
    void fetch_transactions(const code& ec, const hash_list& hashes,
        block_ptr block, uint64_t height,
        block_chain::block_fetch_handler handler)
    {
        if (ec)
        {
            handler(ec, nullptr, 0);
            return;
        }

        BITCOIN_ASSERT(hashes.size() == block->transactions.size());
        BITCOIN_ASSERT(hashes.size() == block->header.transaction_count);

        // This will be called exactly once by the synchronizer.
        const auto completion_handler =
            std::bind(&block_fetcher::handle_complete,
                shared_from_this(), _1, _2, _3, handler);

        // Synchronize transaction fetch calls to one completion call.
        const auto complete = synchronize(completion_handler, hashes.size(),
            "block_fetcher");

        // blockchain::fetch_transaction is thread safe.
        size_t index = 0;
        for (const auto& hash: hashes)
            blockchain_.fetch_transaction(hash,
                std::bind(&block_fetcher::handle_fetch_transaction,
                    shared_from_this(), _1, _2, _3, index++, block, height,
                        complete));
    }

    // Avoid tx copy by swapping chain:tx and (chain)message::tx.
    inline void swapper(chain::transaction& left, chain::transaction& right)
    {
        std::swap(left, right);
    }

    // block_ptr and transaction_ptr must be non-const.
    void handle_fetch_transaction(const code& ec, transaction_ptr transaction,
        uint64_t DEBUG_ONLY(tx_height), size_t index, block_ptr block,
        uint64_t block_height, block_chain::block_fetch_handler handler)
    {
        if (ec)
        {
            handler(ec, nullptr, 0);
            return;
        }

        BITCOIN_ASSERT(tx_height == block_height);

        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        mutex_.lock();
        swapper(block->transactions[index], *transaction);
        mutex_.unlock();
        ///////////////////////////////////////////////////////////////////////

        handler(error::success, block, block_height);
    }

    // If ec success then there is no possibility that block is being written.
    void handle_complete(const code& ec, block_ptr block, uint64_t height,
        block_chain::block_fetch_handler handler)
    {
        if (ec)
        {
            handler(ec, nullptr, 0);
            return;
        }

        handler(error::success, block, height);
    }

    mutable shared_mutex mutex_;
    const block_chain& blockchain_;
};

void fetch_block(const block_chain& chain, uint64_t height,
    block_chain::block_fetch_handler handle_fetch)
{
    // TODO: If concurrent then handler would need to capture fetcher.
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(height, handle_fetch);
}

void fetch_block(const block_chain& chain, const hash_digest& hash,
    block_chain::block_fetch_handler handle_fetch)
{
    // TODO: If concurrent then handler would need to capture fetcher.
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(hash, handle_fetch);
}

} // namespace blockchain
} // namespace libbitcoin
