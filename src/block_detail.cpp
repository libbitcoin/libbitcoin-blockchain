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
    actual_block(std::make_shared<chain::block>(actual_block))
{
}

block_detail::block_detail(const chain::block_header& actual_block_header)
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

orphans_pool::orphans_pool(size_t pool_size)
  : pool_(pool_size)
{
}

bool orphans_pool::add(block_detail_ptr incoming_block)
{
    BITCOIN_ASSERT(incoming_block);

    const auto& incoming_header = incoming_block->actual().header;
    for (auto current_block : pool_)
    {
        // No duplicates allowed.
        const auto& actual = current_block->actual().header;
        if (current_block->actual().header == incoming_header)
            return false;
    }

    pool_.push_back(incoming_block);

    return true;
}

block_detail_list orphans_pool::trace(block_detail_ptr end_block)
{
    BITCOIN_ASSERT(end_block);
    block_detail_list traced_chain;
    traced_chain.push_back(end_block);

    for (auto found = true; found;)
    {
        const auto& actual = traced_chain.back()->actual();
        const auto& previous_block_hash = actual.header.previous_block_hash;
        found = false;

        for (const auto current_block: pool_)
        {
            if (current_block->hash() == previous_block_hash)
            {
                found = true;
                traced_chain.push_back(current_block);
                break;
            }
        }
    }

    BITCOIN_ASSERT(traced_chain.size() > 0);
    std::reverse(traced_chain.begin(), traced_chain.end());
    return traced_chain;
}

block_detail_list orphans_pool::unprocessed()
{
    block_detail_list unprocessed_blocks;
    for (const auto current_block: pool_)
        if (!current_block->is_processed())
            unprocessed_blocks.push_back(current_block);

    // Earlier blocks come into pool first. Lets match that
    // Helps avoid fragmentation, but isn't neccessary
    std::reverse(unprocessed_blocks.begin(), unprocessed_blocks.end());
    return unprocessed_blocks;
}

void orphans_pool::remove(block_detail_ptr remove_block)
{
    BITCOIN_ASSERT(remove_block);
    auto it = std::find(pool_.begin(), pool_.end(), remove_block);
    BITCOIN_ASSERT(it != pool_.end());
    pool_.erase(it);
}

} // namespace blockchain
} // namespace libbitcoin
