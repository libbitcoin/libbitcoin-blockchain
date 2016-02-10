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
#ifndef LIBBITCOIN_BLOCKCHAIN_orphan_pool_HPP
#define LIBBITCOIN_BLOCKCHAIN_orphan_pool_HPP

#include <cstddef>
#include <memory>
#include <boost/circular_buffer.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_detail.hpp>

namespace libbitcoin {
namespace blockchain {

/// An unordered memory pool for orphan blocks
class BCB_API orphan_pool
{
public:
    typedef std::shared_ptr<orphan_pool> ptr;

    orphan_pool(size_t capacity);

    bool add(block_detail::ptr block);
    void remove(block_detail::ptr block);
    block_detail::list trace(block_detail::ptr end);
    block_detail::list unprocessed();

private:
    boost::circular_buffer<block_detail::ptr> buffer_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
