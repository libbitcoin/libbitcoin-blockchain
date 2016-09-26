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
#include <bitcoin/blockchain/validation/fork.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;

fork::fork(size_t capacity)
  : height_(0)
{
    blocks_.reserve(capacity);
}

void fork::set_height(size_t height)
{
    height_ = height;
}

// Push later blocks onto the back, pointing to parent/preceding blocks.
// The front block will always be first after the top of the chain.
bool fork::push(block_const_ptr block)
{
    if (blocks_.empty() ||
        blocks_.back()->hash() == block->header.previous_block_hash)
    {
        blocks_.push_back(block);
        return true;
    }

    return false;
}

// Index is unguarded, caller must verify.
block_const_ptr_list fork::pop(size_t index, const code& reason)
{
    BITCOIN_ASSERT(index < blocks_.size());
    const auto end = blocks_.end();
    const auto start = blocks_.begin() + index;

    block_const_ptr_list out;
    out.reserve(std::distance(start, end));

    for (auto it = start; it != end; ++it)
    {
        const auto block = *it;
        block->metadata.validation_height = block::metadata::orphan_height;
        block->metadata.validation_result = it == start ? reason :
            error::previous_block_invalid;
        out.push_back(block);
    }

    blocks_.erase(start, end);
    blocks_.shrink_to_fit();
    return out;
}

void fork::clear()
{
    blocks_.clear();
    blocks_.shrink_to_fit();
    height_ = 0;
}

// Index is unguarded, caller must verify.
void fork::set_verified(size_t index)
{
    BITCOIN_ASSERT(index < blocks_.size());
    const auto block = blocks_[index];
    block->metadata.validation_height = height_at(index);
    block->metadata.validation_result = error::success;
}

// Index is unguarded, caller must verify.
bool fork::is_verified(size_t index) const
{
    BITCOIN_ASSERT(index < blocks_.size());
    const auto block = blocks_[index];
    return (block->metadata.validation_result == error::success &&
        block->metadata.validation_height == height_at(index));
}

const block_const_ptr_list& fork::blocks() const
{
    return blocks_;
}

bool fork::empty() const
{
    return blocks_.empty();
}

size_t fork::size() const
{
    return blocks_.size();
}

size_t fork::height() const
{
    return height_;
}

hash_digest fork::hash() const
{
    return blocks_.empty() ? null_hash :
        blocks_.front()->header.previous_block_hash;
}

// Index is unguarded, caller must verify.
// Calculate the blockchain height of the block at the given index.
size_t fork::height_at(size_t index) const
{
    const auto fork_height = height();
    BITCOIN_ASSERT(fork_height <= max_size_t - index);
    BITCOIN_ASSERT(fork_height + index <= max_size_t - 1);

    // The height of the blockchain fork point plus zero-based orphan index.
    return fork_height + index + 1;
}

// Index is unguarded, caller must verify.
block_const_ptr fork::block_at(size_t index) const
{
    return index < size() ? blocks_[index] : nullptr;
}

hash_number fork::difficulty() const
{
    hash_number total;

    for (auto& block: blocks_)
        total += block->difficulty();

    return total;
}

} // namespace blockchain
} // namespace libbitcoin
