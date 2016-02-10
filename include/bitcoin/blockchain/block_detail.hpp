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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_DETAIL_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_DETAIL_HPP

#include <memory>
#include <system_error>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_info.hpp>

namespace libbitcoin {
namespace blockchain {

// Metadata + block
class BCB_API block_detail
{
public:
    typedef std::shared_ptr<block_detail> ptr;
    typedef std::vector<block_detail::ptr> list;

    block_detail(chain::block::ptr actual_block);
    block_detail(const chain::block& actual_block);
    block_detail(const chain::header& actual_block_header);
    chain::block& actual();
    const chain::block& actual() const;
    chain::block::ptr actual_ptr() const;
    void mark_processed();
    bool is_processed();
    const hash_digest& hash() const;
    void set_info(const block_info& replace_info);
    const block_info& info() const;
    void set_error(const code& code);
    const code& error() const;

private:
    code code_;
    bool processed_;
    block_info info_;
    const hash_digest block_hash_;
    chain::block::ptr actual_block_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
