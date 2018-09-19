/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;

// This value should never be read, but may be useful in debugging.
static constexpr uint32_t unspecified_timestamp = max_uint32;
static constexpr uint32_t hour_seconds = 3600u;

// Database access is limited to { top, hash, bits, version, timestamp }.

populate_chain_state::populate_chain_state(const fast_chain& chain,
    const settings& settings, const bc::settings& bitcoin_settings)
  : forks_(settings.enabled_forks()),
    stale_seconds_(settings.notify_limit_hours * hour_seconds),
    checkpoints_(config::checkpoint::sort(settings.checkpoints)),
    bitcoin_settings_(bitcoin_settings),
    fast_chain_(chain)
{
}

bool populate_chain_state::get_bits(uint32_t& bits, size_t height,
    const header& header, size_t header_height, bool candidate) const
{
    if (height == header_height)
    {
        bits = header.bits();
        return true;
    }

    return fast_chain_.get_bits(bits, height, candidate);
}

bool populate_chain_state::get_version(uint32_t& version, size_t height,
    const header& header, size_t header_height, bool candidate) const
{
    if (height == header_height)
    {
        version = header.version();
        return true;
    }

    return fast_chain_.get_version(version, height, candidate);
}

bool populate_chain_state::get_timestamp(uint32_t& time, size_t height,
    const header& header, size_t header_height, bool candidate) const
{
    if (height == header_height)
    {
        time = header.timestamp();
        return true;
    }

    return fast_chain_.get_timestamp(time, height, candidate);
}

bool populate_chain_state::get_block_hash(hash_digest& hash, size_t height,
    const header& header, size_t header_height, bool candidate) const
{
    if (height == header_height)
    {
        hash = header.hash();
        return true;
    }

    return fast_chain_.get_block_hash(hash, height, candidate);
}

bool populate_chain_state::populate_bits(chain_state::data& data,
    const chain_state::map& map, const header& header, size_t header_height,
    bool confirmed) const
{
    auto& bits = data.bits.ordered;
    bits.resize(map.bits.count);
    auto height = map.bits.high - map.bits.count;

    for (auto& bit: bits)
        if (!get_bits(bit, ++height,
            header, header_height, confirmed))
            return false;

    return get_bits(data.bits.self, map.bits_self,
        header, header_height, confirmed);
}

bool populate_chain_state::populate_versions(chain_state::data& data,
    const chain_state::map& map, const header& header, size_t header_height,
    bool candidate) const
{
    auto& versions = data.version.ordered;
    versions.resize(map.version.count);
    auto height = map.version.high - map.version.count;

    for (auto& version: versions)
        if (!get_version(version, ++height,
            header, header_height, candidate))
            return false;

    return get_version(data.version.self, map.version_self,
        header, header_height, candidate);
}

bool populate_chain_state::populate_timestamps(chain_state::data& data,
    const chain_state::map& map, const header& header, size_t header_height,
    bool candidate) const
{
    data.timestamp.retarget = unspecified_timestamp;
    auto& timestamps = data.timestamp.ordered;
    timestamps.resize(map.timestamp.count);
    auto height = map.timestamp.high - map.timestamp.count;

    for (auto& timestamp: timestamps)
        if (!get_timestamp(timestamp, ++height,
            header, header_height, candidate))
            return false;

    // Retarget is required if timestamp_retarget is not unrequested.
    if (map.timestamp_retarget != chain_state::map::unrequested &&
        !get_timestamp(data.timestamp.retarget, map.timestamp_retarget,
            header, header_height, candidate))
    {
        return false;
    }

    return get_timestamp(data.timestamp.self, map.timestamp_self,
        header, header_height, candidate);
}

bool populate_chain_state::populate_bip9_bit0(chain_state::data& data,
    const chain_state::map& map, const header& header, size_t header_height,
    bool candidate) const
{
    if (map.bip9_bit0_height == chain_state::map::unrequested)
    {
        data.bip9_bit0_hash = null_hash;
        return true;
    }

    return get_block_hash(data.bip9_bit0_hash, map.bip9_bit0_height,
        header, header_height, candidate);
}

bool populate_chain_state::populate_bip9_bit1(chain_state::data& data,
    const chain_state::map& map, const header& header, size_t header_height,
    bool candidate) const
{
    if (map.bip9_bit1_height == chain_state::map::unrequested)
    {
        data.bip9_bit1_hash = null_hash;
        return true;
    }

    return get_block_hash(data.bip9_bit1_hash, map.bip9_bit1_height,
        header, header_height, candidate);
}

bool populate_chain_state::populate_all(chain_state::data& data,
    const header& header, size_t header_height, bool candidate) const
{
    // Construct the map to inform chain state data population.
    const auto map = chain_state::get_map(data.height, checkpoints_, forks_,
        bitcoin_settings_.retargeting_interval(),
        bitcoin_settings_.activation_sample,
        bitcoin_settings_.bip9_bit0_active_checkpoint,
        bitcoin_settings_.bip9_bit1_active_checkpoint);

    return
        populate_bits(data, map, header, header_height, candidate) &&
        populate_versions(data, map, header, header_height, candidate) &&
        populate_timestamps(data, map, header, header_height, candidate) &&
        populate_bip9_bit0(data, map, header, header_height, candidate) &&
        populate_bip9_bit1(data, map, header, header_height, candidate);
}

// Populate chain state for the top block|header.
chain_state::ptr populate_chain_state::populate(bool candidate) const
{
    header header;
    size_t header_height;

    return fast_chain_.get_top(header, header_height, candidate) ?
        populate(header, header_height, candidate) : nullptr;
}

// Get chain state for the given block|header by height.
chain_state::ptr populate_chain_state::populate(size_t header_height,
    bool candidate) const
{
    header header;

    return fast_chain_.get_header(header, header_height, candidate) ?
        populate(header, header_height, candidate) : nullptr;
}

// Get chain state for the given block|header.
// Only hash and height are queried from the current block/header.
chain_state::ptr populate_chain_state::populate(const header& header,
    size_t header_height, bool candidate) const
{
    chain_state::data data;
    data.height = header_height;
    data.hash = header.hash();

    return populate_all(data, header, header_height, candidate) ?
        std::make_shared<chain_state>(std::move(data), checkpoints_,
            forks_, stale_seconds_, bitcoin_settings_) : nullptr;
}

} // namespace blockchain
} // namespace libbitcoin
