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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_FETCHER_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_FETCHER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;

class BCB_API block_fetcher
  : public std::enable_shared_from_this<block_fetcher>
{
public:
    typedef handle1<std::shared_ptr<chain::block>> handler;

    static void fetch(block_chain& chain, uint64_t height, handler handler);
    static void fetch(block_chain& chain, const hash_digest& hash,
        handler handler);

    block_fetcher(block_chain& chain);

    template <typename BlockIndex>
    void start(const BlockIndex& index, handler handle_fetch)
    {
        // Create the block.
        const auto block = std::make_shared<chain::block>();

        blockchain_.fetch_block_header(index,
            std::bind(&block_fetcher::handle_fetch_header,
                shared_from_this(), _1, _2, block, handle_fetch));
    }

private:
    typedef std::shared_ptr<chain::block> block_ptr;

    void handle_fetch_header(const std::error_code& ec,
        const chain::header& header, block_ptr block,
        handler handle_fetch);

    void fetch_transactions(const std::error_code& ec,
        const hash_list& hashes, block_ptr block, handler handle_fetch);

    void handle_fetch_transaction(const std::error_code& ec,
        const chain::transaction& transaction, size_t index, block_ptr block,
        handler handle_fetch);

    void handle_complete(const std::error_code& ec, block_ptr block,
        handler completion_handler);

    std::mutex mutex_;
    block_chain& blockchain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
