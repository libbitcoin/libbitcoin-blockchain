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
#include <bitcoin/blockchain/block_locator_fetcher.hpp>

#include <cstddef>
#include <memory>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;

void block_locator_fetcher::fetch(block_chain& blockchain,
    handler handle_fetch)
{
    const auto fetcher = std::make_shared<block_locator_fetcher>(blockchain);
    fetcher->start(handle_fetch);
}

block_locator_fetcher::block_locator_fetcher(block_chain& chain)
  : blockchain_(chain), stopped_(false)
{
}

void block_locator_fetcher::start(handler handle_fetch)
{
    handler_ = handle_fetch;
    blockchain_.fetch_last_height(
        std::bind(&block_locator_fetcher::populate,
            shared_from_this(), _1, _2));
}


bool block_locator_fetcher::stop_on_error(const code& ec)
{
    if (stopped_)
        return true;

    if (ec)
    {
        handler_(ec, message::block_locator());
        return true;
    }

    return false;
}

void block_locator_fetcher::loop()
{
    // Stop looping and finish.
    if (indexes_.empty())
    {
        handler_(error::success, locator_);
        return;
    }

    const auto self = shared_from_this();
    const auto height = indexes_.back();
    indexes_.pop_back();
    blockchain_.fetch_block_header(height,
        std::bind(&block_locator_fetcher::append,
        self, _1, _2, height));
}

void block_locator_fetcher::populate(const code& ec, size_t last_height)
{
    if (stop_on_error(ec))
        return;

    indexes_ = block_locator_indexes(last_height);

    // We reverse our list so we can pop items off the top
    // as we need to get them, and push items to our locator.
    // The order of items in the locator should match
    // the order of items in our index.
    std::reverse(indexes_.begin(), indexes_.end());
    loop();
}

void block_locator_fetcher::append(const code& ec, const chain::header& header,
    size_t /* height */)
{
    if (stop_on_error(ec))
        return;

    const auto block_hash = header.hash();
    locator_.push_back(block_hash);

    // Continue the loop.
    loop();
}

} // namespace chain
} // namespace libbitcoin
