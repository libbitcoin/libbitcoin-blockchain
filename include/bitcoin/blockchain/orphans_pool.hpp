/**
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_ORPHANS_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_ORPHANS_POOL_HPP

#include <cstddef>
#include <memory>
#include <boost/circular_buffer.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_detail.hpp>

namespace libbitcoin {
namespace chain {

// An unordered memory pool for orphan blocks
class BCB_API orphans_pool
{
public:
    orphans_pool(size_t size=20);

    bool empty() const;
    size_t size() const;

    bool add(block_detail_ptr incoming_block);
    void remove(block_detail_ptr remove_block);
    block_detail_list trace(block_detail_ptr end_block);
    block_detail_list unprocessed();

private:
    boost::circular_buffer<block_detail_ptr> buffer_;
};

// TODO: define in orphans_pool (compat break).
typedef std::shared_ptr<orphans_pool> orphans_pool_ptr;

} // namespace chain
} // namespace libbitcoin

#endif
