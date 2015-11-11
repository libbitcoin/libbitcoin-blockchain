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

#include <atomic>
#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API block_fetcher
  : public std::enable_shared_from_this<block_fetcher>
{
public:
    typedef block_chain::block_fetch_handler handler;

    static void fetch(block_chain& chain, uint64_t height,
        handler handle_fetch);
    static void fetch(block_chain& chain, const hash_digest& hash,
        handler handle_fetch);

    block_fetcher(block_chain& chain);

    template <typename BlockIndex>
    void start(const BlockIndex& index, handler handle_fetch)
    {
        // Keep the class in scope until this handler completes.
        const auto self = shared_from_this();
        const auto handle_fetch_header = [self](const code ec,
            const chain::header block_header)
        {
            if (self->stop_on_error(ec))
                return;

            self->block_.header = block_header;
            //self->fetch_hashes();
        };

        handler_ = handle_fetch;
        blockchain_.fetch_block_header(index, handle_fetch_header);
    }

private:
    bool stop_on_error(const code& ec);
    void fetch_tx(const hash_digest& tx_hash, size_t tx_index);
    void fetch_transactions(const code& ec, const hash_list& tx_hashes);
    //void fetch_hashes();

    handler handler_;
    chain::block block_;
    block_chain& blockchain_;
    std::atomic<size_t> handled_count_;
    bool stopped_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
