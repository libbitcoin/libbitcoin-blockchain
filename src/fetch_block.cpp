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
#include <bitcoin/blockchain/fetch_block.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <system_error>
#include <bitcoin/blockchain/blockchain.hpp>

namespace libbitcoin {
namespace chain {

using std::placeholders::_1;
using std::placeholders::_2;

// This is just an alias until we rename blockchain_fetch_handler_block.
typedef blockchain_fetch_handler_block block_fetch_handler;

// This class is used only locally.
class block_fetcher
  : public std::enable_shared_from_this<block_fetcher>
{
public:
    block_fetcher(blockchain& chain)
      : blockchain_(chain), stopped_(false)
    {
    }

    template <typename BlockIndex>
    void start(const BlockIndex& index, block_fetch_handler handler)
    {
        // Keep the class in scope until this handler completes.
        const auto self = shared_from_this();
        const auto handle_fetch_header = [self](const std::error_code& ec,
            const block_header_type& block_header)
        {
            if (self->stop_on_error(ec))
                return;

            self->block_.header = block_header;
            //self->fetch_hashes();
        };

        handler_ = handler;
        blockchain_.fetch_block_header(index, handle_fetch_header);
    }

private:
    bool stop_on_error(const std::error_code& ec)
    {
        if (stopped_)
            return true;

        if (ec)
        {
            stopped_ = true;
            handler_(ec, block_type());
            return true;
        }

        return false;
    }

    //void fetch_hashes()
    //{
    //    blockchain_.fetch_block_transaction_hashes(
    //        hash_block_header(block_.header),
    //            std::bind(&fetch_block_t::fetch_transactions,
    //                shared_from_this(), _1, _2));
    //}

    void fetch_transactions(const std::error_code& ec,
        const hash_list& tx_hashes)
    {
        if (stop_on_error(ec))
            return;

        block_.transactions.resize(tx_hashes.size());
        count_ = 0;
        for (size_t tx_index = 0; tx_index < tx_hashes.size(); ++tx_index)
            fetch_tx(tx_hashes[tx_index], tx_index);
    }

    void fetch_tx(const hash_digest& tx_hash, size_t tx_index)
    {
        const auto handle_fetch = [this, tx_index](const std::error_code& ec,
            const transaction_type& tx)
        {
            if (stop_on_error(ec))
                return;

            BITCOIN_ASSERT(tx_index < block_.transactions.size());
            block_.transactions[tx_index] = tx;
            ++count_;
            if (count_ == block_.transactions.size())
                handler_(error::success, block_);
        };

        blockchain_.fetch_transaction(tx_hash, handle_fetch);
    }

    blockchain& blockchain_;
    block_type block_;
    atomic_counter count_;
    block_fetch_handler handler_;

    // TODO: use lock-free std::atomic_flag?
    std::atomic<bool> stopped_;
};

void fetch_block(blockchain& chain, size_t height,
    blockchain_fetch_handler_block handle_fetch)
{
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(height, handle_fetch);
}

void fetch_block(blockchain& chain, const hash_digest& block_hash,
    blockchain_fetch_handler_block handle_fetch)
{
    const auto fetcher = std::make_shared<block_fetcher>(chain);
    fetcher->start(block_hash, handle_fetch);
}

} // namespace chain
} // namespace libbitcoin
