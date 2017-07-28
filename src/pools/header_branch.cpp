/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/pools/header_branch.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;

header_branch::header_branch(size_t height)
  : height_(height),
    headers_(std::make_shared<header_const_ptr_list>())
{
}

void header_branch::set_height(size_t height)
{
    height_ = height;
}

// Front is the top of the chain plus one, back is the top of the branch.
bool header_branch::push(header_const_ptr header)
{
    const auto linked = [this](header_const_ptr header)
    {
        return headers_->front()->previous_block_hash() == header->hash();
    };

    if (empty() || linked(header))
    {
        // TODO: optimize by preventing vector reallocations here.
        headers_->insert(headers_->begin(), header);
        return true;
    }

    return false;
}

header_const_ptr header_branch::top_parent() const
{
    const auto count = size();
    return count < 2 ? nullptr : (*headers_)[count - 2u];
}

header_const_ptr header_branch::top() const
{
    return empty() ? nullptr : headers_->back();
}

size_t header_branch::top_height() const
{
    return height() + size();
}

header_const_ptr_list_const_ptr header_branch::headers() const
{
    return headers_;
}

bool header_branch::empty() const
{
    return headers_->empty();
}

size_t header_branch::size() const
{
    return headers_->size();
}

size_t header_branch::height() const
{
    return height_;
}

hash_digest header_branch::hash() const
{
    return empty() ? null_hash :
        headers_->front()->previous_block_hash();
}

config::checkpoint header_branch::fork_point() const
{
    return{ hash(), height() };
}

// private
size_t header_branch::index_of(size_t height) const
{
    // The member height_ is height of the fork point, not the first header.
    return safe_subtract(safe_subtract(height, height_), size_t(1));
}

// private
size_t header_branch::height_at(size_t index) const
{
    // The height of the blockchain branch point plus zero-based index.
    return safe_add(safe_add(index, height_), size_t(1));
}

 // The branch work check is both a consensus check and denial of service
 // protection. It is necessary here total claimed work exceeds that of the
 // competing chain segment (consensus), and that the work has actually been
 // expended (denial of service protection). The latter ensures we don't query
 // the chain for total segment work path the branch competetiveness. Once work
 // is proven sufficient the blocks are validated, requiring each to have the
 // work required by the header accept check. It is possible that longer chain
 // of lower work blocks could meet both above criteria. However this requires
 // the same amount of work as a shorter segment, so an attacker gains no
 // advantage from that option, and it will be caught in validation.
uint256_t header_branch::work() const
{
    uint256_t total;

    // Not using accumulator here avoids repeated copying of uint256 object.
    for (auto header: *headers_)
        total += header->proof();

    return total;
}

// The bits of the header at the given height in the branch.
bool header_branch::get_bits(uint32_t& out_bits, size_t height) const
{
    if (height <= height_)
        return false;

    const auto header = (*headers_)[index_of(height)];

    if (!header)
        return false;

    out_bits = header->bits();
    return true;
}

// The version of the header at the given height in the branch.
bool header_branch::get_version(uint32_t& out_version, size_t height) const
{
    if (height <= height_)
        return false;

    const auto header = (*headers_)[index_of(height)];

    if (!header)
        return false;

    out_version = header->version();
    return true;
}

// The timestamp of the header at the given height in the branch.
bool header_branch::get_timestamp(uint32_t& out_timestamp, size_t height) const
{
    if (height <= height_)
        return false;

    const auto header = (*headers_)[index_of(height)];

    if (!header)
        return false;

    out_timestamp = header->timestamp();
    return true;
}

// The hash of the header at the given height if it exists in the branch.
bool header_branch::get_block_hash(hash_digest& out_hash, size_t height) const
{
    if (height <= height_)
        return false;

    const auto header = (*headers_)[index_of(height)];

    if (!header)
        return false;

    out_hash = header->hash();
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
