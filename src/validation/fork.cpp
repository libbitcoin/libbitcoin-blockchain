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
#include <numeric>
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
        blocks_.back()->hash() == block->header().previous_block_hash())
    {
        blocks_.push_back(block);
        return true;
    }

    return false;
}

// Index is unguarded, caller must verify.
block_const_ptr_list fork::pop(size_t index, const code& reason)
{
    const auto end = blocks_.end();
    const auto start = blocks_.begin() + index;

    block_const_ptr_list out;
    out.reserve(std::distance(start, end));

    for (auto it = start; it != end; ++it)
    {
        const auto block = *it;
        block->validation.height = block::validation::orphan_height;
        block->validation.result = it == start ? reason :
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

// Index is unguarded, caller must verify or access violation will result.
void fork::set_verified(size_t index) const
{
    BITCOIN_ASSERT(index < blocks_.size());
    const auto block = blocks_[index];
    block->validation.height = height_at(index);
    block->validation.result = error::success;
}

// Index is unguarded, caller must verify or access violation will result.
bool fork::is_verified(size_t index) const
{
    BITCOIN_ASSERT(index < blocks_.size());
    const auto block = blocks_[index];
    return (block->validation.result == error::success &&
        block->validation.height == height_at(index));
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
        blocks_.front()->header().previous_block_hash();
}

// The caller must ensure that the height is above the fork.
size_t fork::index_of(size_t height) const
{
    return safe_subtract(safe_subtract(height, height_), size_t(1));
}

// Index is unguarded, caller must verify.
size_t fork::height_at(size_t index) const
{
    // The height of the blockchain fork point plus zero-based orphan index.
    return safe_add(safe_add(height_, index), size_t(1));
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

// Excluding self and early termination is harder than just counting all.
void fork::populate_tx(size_t index, const chain::transaction& tx) const
{
    const auto outer = [&tx](size_t total, block_const_ptr block)
    {
        const auto hashes = [&tx](const transaction& block_tx)
        {
            return block_tx.hash() == tx.hash();
        };

        const auto& txs = block->transactions();
        return total + std::count_if(txs.begin(), txs.end(), hashes);
    };

    const auto& end = blocks_.begin() + index + 1u;
    const auto count = std::accumulate(blocks_.begin(), end, size_t(0), outer);

    tx.validation.duplicate = count > 1u;
}

// Excluding self and early termination is harder than just counting all.
void fork::populate_spent(size_t index, const output_point& outpoint) const
{
    const auto outer = [&outpoint](size_t total, block_const_ptr block)
    {
        const auto inner = [&outpoint](size_t sum, const transaction& tx)
        {
            const auto points = [&outpoint](const input& input)
            {
                return input.previous_output() == outpoint;
            };

            const auto& ins = tx.inputs();
            return sum + std::count_if(ins.begin(), ins.end(), points);
        };

        const auto& txs = block->transactions();
        return total + std::accumulate(txs.begin(), txs.end(), total, inner);
    };

    const auto& end = blocks_.begin() + index + 1u;
    const auto spent = std::accumulate(blocks_.begin(), end, size_t(0), outer);

    auto& prevout = outpoint.validation;
    prevout.spent = spent > 1u;
    prevout.confirmed = prevout.spent;
}

void fork::populate_prevout(size_t index, const output_point& outpoint) const
{
    auto& prevout = outpoint.validation;
    struct result { size_t height; size_t position;  output out; };

    const auto get_output = [this, &outpoint, index]() -> result
    {
        for (size_t forward = 0; forward <= index; ++forward)
        {
            const auto reverse = index - forward;
            const auto& txs = block_at(reverse)->transactions();

            for (size_t position = 0; position < txs.size(); ++position)
            {
                const auto& tx = txs[position];

                if (outpoint.hash() == tx.hash() &&
                    outpoint.index() < tx.outputs().size())
                {
                    return
                    {
                        height_at(reverse),
                        position,
                        tx.outputs()[outpoint.index()]
                    };
                }
            }
        }

        return{};
    };

    // In case this input is a coinbase or the prevout is spent.
    prevout.cache.reset();

    // The height of the prevout must be set iff the prevout is coinbase.
    prevout.height = output_point::validation::not_specified;

    // The input is a coinbase, so there is no prevout to populate.
    if (outpoint.is_null())
        return;

    ///////////////////////////////////////////////////////////////////////////
    // We continue even if prevout spent and/or missing.
    ///////////////////////////////////////////////////////////////////////////

    // Get the script and value for the prevout.
    const auto finder = get_output();

    // Found the prevout at or below the top of the fork.
    if (!finder.out.is_valid())
        return;

    prevout.cache = finder.out;

    // Set height iff the prevout is coinbase (first tx is coinbase).
    if (finder.position == 0)
        prevout.height = finder.height;
}

/// The bits of the block at the given height in the fork.
bool fork::get_bits(uint32_t out_bits, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = block_at(index_of(height));

    if (!block)
        return false;

    out_bits = block->header().bits();
    return true;
}

/// The version of the block at the given height in the fork.
bool fork::get_version(uint32_t out_version, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = block_at(index_of(height));

    if (!block)
        return false;

    out_version = block->header().version();
    return true;
}

/// The timestamp of the block at the given height in the fork.
bool fork::get_timestamp(uint32_t out_timestamp, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = block_at(index_of(height));

    if (!block)
        return false;

    out_timestamp = block->header().timestamp();
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
