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
#include <bitcoin/blockchain/fetch_block_locator.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <system_error>
#include <bitcoin/blockchain/blockchain.hpp>

namespace libbitcoin {
namespace chain {

using std::placeholders::_1;
using std::placeholders::_2;

// This is just an alias until we rename blockchain_fetch_handler_block_locator
typedef blockchain_fetch_handler_block_locator block_locator_fetch_handler;

// This class that is used only locally.
class block_locator_fetcher
  : public std::enable_shared_from_this<block_locator_fetcher>
{
public:
    block_locator_fetcher(blockchain& chain)
      : blockchain_(chain)
    {
    }

    void start(block_locator_fetch_handler handler)
    {
        handler_ = handler;
        blockchain_.fetch_last_height(
            std::bind(&block_locator_fetcher::populate,
                shared_from_this(), _1, _2));
    }

private:
    bool stop_on_error(const std::error_code& ec)
    {
        if (ec)
        {
            handler_(ec, block_locator_type());
            return true;
        }

        return false;
    }

    void populate(const std::error_code& ec, size_t last_height)
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

    void loop()
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

    void append(const std::error_code& ec, const block_header_type& header,
        size_t /* height */)
    {
        if (stop_on_error(ec))
            return;

        const auto block_hash = hash_block_header(header);
        locator_.push_back(block_hash);

        // Continue the loop.
        loop();
    }

    blockchain& blockchain_;
    index_list indexes_;
    block_locator_type locator_;
    block_locator_fetch_handler handler_;
};

void fetch_block_locator(blockchain& blockchain, 
    blockchain_fetch_handler_block_locator handle_fetch)
{
    const auto fetcher = std::make_shared<block_locator_fetcher>(blockchain);
    fetcher->start(handle_fetch);
}

} // namespace chain
} // namespace libbitcoin

