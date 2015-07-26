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

namespace libbitcoin {
namespace chain {

// Metadata + block
class BCB_API block_detail
{
public:
    block_detail(const block_type& actual_block);
    block_detail(const block_header_type& actual_block_header);
    block_type& actual();
    const block_type& actual() const;
    std::shared_ptr<block_type> actual_ptr() const;
    void mark_processed();
    bool is_processed();
    const hash_digest& hash() const;
    void set_info(const block_info& replace_info);
    const block_info& info() const;
    void set_error(const std::error_code& code);
    const std::error_code& error() const;

private:
    std::shared_ptr<block_type> actual_block_;
    const hash_digest block_hash_;
    bool processed_;
    block_info info_;
    std::error_code code_;
};

// TODO: define in block_detail (compat break).
typedef std::shared_ptr<block_detail> block_detail_ptr;
typedef std::vector<block_detail_ptr> block_detail_list;

} // namespace chain
} // namespace libbitcoin

#endif
