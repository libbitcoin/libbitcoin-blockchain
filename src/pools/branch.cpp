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
#include <bitcoin/blockchain/pools/branch.hpp>

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

branch::branch(size_t height)
  : height_(height),
    blocks_(std::make_shared<block_const_ptr_list>())
{
    blocks_->reserve(1);
}

void branch::set_height(size_t height)
{
    height_ = height;
}

// Front is the top of the chain plus one, back is the top of the branch.
bool branch::push_front(block_const_ptr block)
{
    const auto linked = [this](block_const_ptr block)
    {
        const auto& front = blocks_->front()->header();
        return front.previous_block_hash() == block->hash();
    };

    if (empty() || linked(block))
    {
        // TODO: optimize.
        blocks_->insert(blocks_->begin(), block);
        return true;
    }

    return false;
}

block_const_ptr branch::top() const
{
    return empty() ? nullptr : blocks_->back();
}

size_t branch::top_height() const
{
    return height() + size();
}

block_const_ptr_list_const_ptr branch::blocks() const
{
    // Protect the blocks list from the caller.
    return std::const_pointer_cast<const block_const_ptr_list>(blocks_);
}

bool branch::empty() const
{
    return blocks_->empty();
}

size_t branch::size() const
{
    return blocks_->size();
}

size_t branch::height() const
{
    return height_;
}

hash_digest branch::hash() const
{
    return empty() ? null_hash : 
        blocks_->front()->header().previous_block_hash();
}

// private
size_t branch::index_of(size_t height) const
{
    // The member height_ is the height of the fork point, not the first block.
    return safe_subtract(safe_subtract(height, height_), size_t(1));
}

// private
size_t branch::height_at(size_t index) const
{
    // The height of the blockchain branch point plus zero-based orphan index.
    return safe_add(safe_add(index, height_), size_t(1));
}

// The branch difficulty check is both a consensus check and denial of service
// protection. It is necessary here that total claimed work exceeds that of the
// competing chain segment (consensus), and that the work has actually been
// expended (denial of service protection). The latter ensures we don't query
// the chain for total segment difficulty path the branch competetiveness.
// Once work is proven sufficient the blocks are validated, requiring each to
// have the work required by the header accept check. It is possible that a
// longer chain of lower work blocks could meet both above criteria. However
// this requires the same amount of work as a shorter segment, so an attacker
// gains no advantage from that option, and it will be caught in validation.
uint256_t branch::difficulty() const
{
    uint256_t total;

    for (auto block: *blocks_)
        total += block->difficulty();

    return total;
}

void branch::populate_tx(const chain::transaction& tx) const
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

    // Counting all is easier than excluding self and terminating early.
    const auto count = std::accumulate(blocks_->begin(), blocks_->end(),
        size_t(0), outer);

    BITCOIN_ASSERT(count > 0);
    tx.validation.duplicate = count > 1u;
}

void branch::populate_spent(const output_point& outpoint) const
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

    // Counting all is easier than excluding self and terminating early.
    const auto spent = std::accumulate(blocks_->begin(), blocks_->end(),
        size_t(0), outer);

    BITCOIN_ASSERT(spent > 0);
    auto& prevout = outpoint.validation;
    prevout.spent = spent > 1u;
    prevout.confirmed = prevout.spent;
}

void branch::populate_prevout(const output_point& outpoint) const
{
    const auto count = size();
    auto& prevout = outpoint.validation;
    struct result { size_t height; size_t position; output out; };

    const auto get_output = [this, count, &outpoint]() -> result
    {
        const auto& blocks = *blocks_;

        // Reverse search because of BIP30.
        for (size_t forward = 0; forward < count; ++forward)
        {
            const size_t index = count - forward - 1u;
            const auto& txs = blocks[index]->transactions();

            for (size_t position = 0; position < txs.size(); ++position)
            {
                const auto& tx = txs[position];

                if (outpoint.hash() == tx.hash() &&
                    outpoint.index() < tx.outputs().size())
                {
                    return
                    {
                        height_at(index),
                        position,
                        tx.outputs()[outpoint.index()]
                    };
                }
            }
        }

        return{};
    };

    // In case this input is a coinbase or the prevout is spent.
    prevout.cache = chain::output{};

    // The height of the prevout must be set iff the prevout is coinbase.
    prevout.height = output_point::validation_type::not_specified;

    // The input is a coinbase, so there is no prevout to populate.
    if (outpoint.is_null())
        return;

    // We continue even if prevout spent and/or missing.

    // Get the script and value for the prevout.
    const auto finder = get_output();

    if (!finder.out.is_valid())
        return;

    // Found the prevout at or below the indexed block.
    prevout.cache = finder.out;

    // Set height iff the prevout is coinbase (first tx is coinbase).
    if (finder.position == 0)
        prevout.height = finder.height;
}

/// The bits of the block at the given height in the branch.
bool branch::get_bits(uint32_t& out_bits, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = (*blocks_)[index_of(height)];

    if (!block)
        return false;

    out_bits = block->header().bits();
    return true;
}

// The version of the block at the given height in the branch.
bool branch::get_version(uint32_t& out_version, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = (*blocks_)[index_of(height)];

    if (!block)
        return false;

    out_version = block->header().version();
    return true;
}

// The timestamp of the block at the given height in the branch.
bool branch::get_timestamp(uint32_t& out_timestamp, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = (*blocks_)[index_of(height)];

    if (!block)
        return false;

    out_timestamp = block->header().timestamp();
    return true;
}

// The hash of the block at the given height if it exists in the branch.
bool branch::get_block_hash(hash_digest& out_hash, size_t height) const
{
    if (height <= height_)
        return false;

    const auto block = (*blocks_)[index_of(height)];

    if (!block)
        return false;

    out_hash = block->hash();
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
