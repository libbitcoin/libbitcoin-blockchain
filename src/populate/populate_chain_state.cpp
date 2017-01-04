/**
 * Copyright (c) 2011-2016 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;

// Use system clock because we require accurate time of day.
using wall_clock = std::chrono::system_clock;

// This value should never be read, but may be useful in debugging.
static constexpr uint32_t unspecified = max_uint32;

// Database access is limited to:
// block: { hash, bits, version, timestamp }

populate_chain_state::populate_chain_state(const fast_chain& chain,
    const settings& settings)
  : block_version_(settings.block_version),
    configured_forks_(settings.enabled_forks),    
    checkpoints_(config::checkpoint::sort(settings.checkpoints)),
    fast_chain_(chain)
{
}

inline std::time_t now()
{
    return wall_clock::to_time_t(wall_clock::now());
}

inline bool is_transaction_pool(branch::const_ptr branch)
{
    return !branch->empty();
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

    return true;
}

bool populate_chain_state::populate_versions(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    auto& versions = data.version.unordered;
    versions.resize(map.version.count);
    auto height = map.version.high - map.version.count;

    for (auto& version: versions)
        if (!get_version(version, ++height, branch))
            return false;

    if (is_transaction_pool(branch))
    {
        data.version.self = block_version_;
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
        data.timestamp.self = now();
        return true;
    }

    return get_timestamp(data.timestamp.self, map.timestamp_self, branch);
}

bool populate_chain_state::populate_checkpoint(chain_state::data& data,
    const chain_state::map& map, branch::const_ptr branch) const
{
    if (map.allowed_duplicates_height == chain_state::map::unrequested)
    {
        // The allowed_duplicates_hash must be null_hash if unrequested.
        data.allowed_duplicates_hash = null_hash;
        return true;
    }

    return get_block_hash(data.allowed_duplicates_hash,
        map.allowed_duplicates_height, branch);
}

bool populate_chain_state::populate_all(chain_state::data& data,
    branch::const_ptr branch) const
{
    // Construct a map to inform chain state data population.
    const auto map = chain_state::get_map(data.height, checkpoints_,
        configured_forks_);

    return (populate_bits(data, map, branch) &&
        populate_versions(data, map, branch) &&
        populate_timestamps(data, map, branch) &&
        populate_checkpoint(data, map, branch));
}

// TODO: create a branch subset of the orphan pool.
chain_state::ptr populate_chain_state::populate() const
{
    size_t last_height;
    
    if (!fast_chain_.get_last_height(last_height))
        return{};

    chain_state::data data;
    data.hash = null_hash;
    data.height = safe_add(last_height, size_t(1));

    // Use an empty branch to represent the transaction pool.
    const auto empty_branch = std::make_shared<branch>(last_height);

    if (!populate_all(data, empty_branch))
        return{};

    return std::make_shared<chain_state>(std::move(data), checkpoints_,
        configured_forks_);
}

// TODO: generate from cache using preceding block's map and data.
chain_state::ptr populate_chain_state::populate(branch::const_ptr branch) const
{
    const auto block = branch->top();
    BITCOIN_ASSERT(block);

    chain_state::data data;
    data.hash = block->hash();
    data.height = branch->top_height();

    if (!populate_all(data, branch))
        return{};

    return std::make_shared<chain_state>(std::move(data), checkpoints_,
        configured_forks_);
}

} // namespace blockchain
} // namespace libbitcoin
