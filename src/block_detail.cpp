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
#include <bitcoin/blockchain/block_detail.hpp>

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace blockchain {

block_detail::block_detail(const chain::block& actual_block)
  : block_hash_(actual_block.header.hash()), processed_(false),
    info_({ block_status::orphan, 0 }),
    actual_block_(std::make_shared<chain::block>(actual_block))
{
}

block_detail::block_detail(const chain::header& actual_block_header)
  : block_detail(chain::block{ actual_block_header, {} })
{
}

chain::block& block_detail::actual()
{
    return *actual_block_;
}

const chain::block& block_detail::actual() const
{
    return *actual_block_;
}

std::shared_ptr<chain::block> block_detail::actual_ptr() const
{
    return actual_block_;
}

void block_detail::mark_processed()
{
    processed_ = true;
}
bool block_detail::is_processed()
{
    return processed_;
}

const hash_digest& block_detail::hash() const
{
    return block_hash_;
}

void block_detail::set_info(const block_info& replace_info)
{
    info_ = replace_info;
}

const block_info& block_detail::info() const
{
    return info_;
}

void block_detail::set_error(const std::error_code& code)
{
    code_ = code;
}

const std::error_code& block_detail::error() const
{
    return code_;
}

} // namespace blockchain
} // namespace libbitcoin
