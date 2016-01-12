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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_LOCATOR_FETCHER_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_LOCATOR_FETCHER_HPP

#include <cstddef>
#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API block_locator_fetcher
  : public std::enable_shared_from_this<block_locator_fetcher>
{
public:
    typedef block_chain::block_locator_fetch_handler handler;

    // TODO: make a threadsafe cache against the last fetched hash.
    static void fetch(block_chain& chain, handler handle_fetch);

    block_locator_fetcher(block_chain& chain);

    void start(handler handle_fetch);

private:
    void loop();
    bool stop_on_error(const code& ec);
    void populate(const code& ec, size_t last_height);
    void append(const code& ec, const chain::header& header);

    block_chain& blockchain_;
    index_list indexes_;
    message::block_locator locator_;
    handler handler_;
    bool stopped_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
