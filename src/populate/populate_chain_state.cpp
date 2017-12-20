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
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;

// This value should never be read, but may be useful in debugging.
static constexpr uint32_t unspecified = max_uint32;

// Database access is limited to:
// get_last_height
// block: { hash, bits, version, timestamp }

populate_chain_state::populate_chain_state(const fast_chain& chain,
    const settings& settings)
  : configured_forks_(settings.enabled_forks()),
    checkpoints_(config::checkpoint::sort(settings.checkpoints)),
    fast_chain_(chain)
{
}

inline bool is_transaction_pool(branch::const_ptr branch)
{
    return branch->empty();
}

bool populate_chain_state::get_bits(uint32_t& out_bits, size_t height,
    branch::const_ptr branch) const
{
    // branch returns false only if the height is out of range.
    return branch->get_bits(out_bits, height) ||
        fast_chain_.get_bits(out_bits, height);
}

bool populate_chain_state::get_version(uint32_t& out_version, size_t height,
    branch::const_ptr branch) const
{
    // branch returns false only if the height is out of range.
    return branch->get_version(out_version, height) ||
        fast_chain_.get_version(out_version, height);
}

bool populate_chain_state::get_timestamp(uint32_t& out_timestamp, size_t height,
    branch::const_ptr branch) const
{
    // branch returns false only if the height is out of range.
    return branch->get_timestamp(out_timestamp, height) ||
        fast_chain_.get_timestamp(out_timestamp, height);
}

bool populate_chain_state::get_block_hash(hash_digest& out_hash, size_t height,
    branch::const_ptr branch) const
{
    return branch->get_block_hash(out_hash, height) ||
        fast_chain_.get_block_hash(out_hash, height);
}

bool populate_chain_state::populate_bits(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    auto& bits = data.bits.ordered;
    bits.resize(map.bits.count);
    auto height = map.bits.high - map.bits.count;

    for (auto& bit: bits)
        if (!get_bits(bit, ++height, branch))
            return false;

    if (is_transaction_pool(branch))
    {
        // This is an unused value.
        data.bits.self = work_limit(true);
        return true;
    }

    return get_bits(data.bits.self, map.bits_self, branch);
}

bool populate_chain_state::populate_versions(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    auto& versions = data.version.ordered;
    versions.resize(map.version.count);
    auto height = map.version.high - map.version.count;

    for (auto& version: versions)
        if (!get_version(version, ++height, branch))
            return false;

    if (is_transaction_pool(branch))
    {
        data.version.self = chain_state::signal_version(configured_forks_);
        return true;
    }

    return get_version(data.version.self, map.version_self, branch);
}

bool populate_chain_state::populate_timestamps(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    data.timestamp.retarget = unspecified;
    auto& timestamps = data.timestamp.ordered;
    timestamps.resize(map.timestamp.count);
    auto height = map.timestamp.high - map.timestamp.count;

    for (auto& timestamp: timestamps)
        if (!get_timestamp(timestamp, ++height, branch))
            return false;

    // Retarget is required if timestamp_retarget is not unrequested.
    if (map.timestamp_retarget != chain_state::map::unrequested &&
        !get_timestamp(data.timestamp.retarget, map.timestamp_retarget, branch))
    {
        return false;
    }

    if (is_transaction_pool(branch))
    {
        data.timestamp.self = static_cast<uint32_t>(zulu_time());
        return true;
    }

    return get_timestamp(data.timestamp.self, map.timestamp_self, branch);
}

bool populate_chain_state::populate_collision(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    if (map.allow_collisions_height == chain_state::map::unrequested)
    {
        data.allow_collisions_hash = null_hash;
        return true;
    }

    return get_block_hash(data.allow_collisions_hash,
        map.allow_collisions_height, branch);
}

bool populate_chain_state::populate_bip9_bit0(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    if (map.bip9_bit0_height == chain_state::map::unrequested)
    {
        data.bip9_bit0_hash = null_hash;
        return true;
    }

    return get_block_hash(data.bip9_bit0_hash,
        map.bip9_bit0_height, branch);
}

bool populate_chain_state::populate_bip9_bit1(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    if (map.bip9_bit1_height == chain_state::map::unrequested)
    {
        data.bip9_bit1_hash = null_hash;
        return true;
    }

    return get_block_hash(data.bip9_bit1_hash,
        map.bip9_bit1_height, branch);
}

bool populate_chain_state::populate_all(chain_state::data& data,
    branch::const_ptr branch) const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    // Construct a map to inform chain state data population.
    const auto map = chain_state::get_map(data.height, checkpoints_,
        configured_forks_);

    return (populate_bits(data, map, branch) &&
        populate_versions(data, map, branch) &&
        populate_timestamps(data, map, branch) &&
        populate_collision(data, map, branch) &&
        populate_bip9_bit0(data, map, branch) &&
        populate_bip9_bit1(data, map, branch));
    ///////////////////////////////////////////////////////////////////////////
}

// Caller should test result, but failure implies store corruption.
chain_state::ptr populate_chain_state::populate() const
{
    size_t top;
    if (!fast_chain_.get_last_height(top))
        return nullptr;

    chain_state::data data;
    data.hash = null_hash;
    data.height = safe_add(top, size_t(1));

    // Use an empty branch to represent the transaction pool.
    if (!populate_all(data, std::make_shared<branch>(top)))
        return nullptr;

    return std::make_shared<chain_state>(std::move(data), checkpoints_,
        configured_forks_);
}

// Caller should test result, but failure implies store corruption.
chain_state::ptr populate_chain_state::populate(const chain_state& pool,
    branch::const_ptr branch) const
{
    const auto block = branch->top();
    BITCOIN_ASSERT(block);

    // If this is not a reorganization we can just promote the pool state.
    if (branch->size() == 1 && branch->top_height() == pool.height())
        return std::make_shared<chain_state>(pool, *block);

    chain_state::data data;
    data.hash = block->hash();
    data.height = branch->top_height();

    if (!populate_all(data, branch))
        return nullptr;

    return std::make_shared<chain_state>(std::move(data), checkpoints_,
        configured_forks_);
}

// Caller should test result, but failure implies store corruption.
chain::chain_state::ptr populate_chain_state::populate(
    const chain_state& parent, header_const_ptr header) const
{
    //=========================================================================
    //=========================================================================
    // TODO: find the header pool entry or chain block of the header.parent.
    // TODO: read or generate the parent chain state and promote to the header.
    // Create header state from parent state.
    return nullptr;
    //=========================================================================
    //=========================================================================
}

} // namespace blockchain
} // namespace libbitcoin
