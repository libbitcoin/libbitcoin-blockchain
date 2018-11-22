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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_CHAIN_STATE_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_CHAIN_STATE_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API populate_chain_state
{
public:
    populate_chain_state(const fast_chain& chain, const settings& settings,
        const system::settings& bitcoin_settings);

    /// Populate chain state for candidate or confirmed top block|header.
    system::chain::chain_state::ptr populate(bool candidate) const;

    /// Populate chain state for candidate or confirmed block|header by height.
    system::chain::chain_state::ptr populate(size_t header_height,
        bool candidate) const;

    /// Populate chain state for the given block|header.
    system::chain::chain_state::ptr populate(
        const system::chain::header& header, size_t header_height,
        bool candidate) const;

private:
    typedef system::chain::header header;
    typedef system::chain::chain_state::map map;
    typedef system::chain::chain_state::data data;

    bool get_bits(uint32_t& bits, size_t height, const header& header,
        size_t header_height, bool candidate) const;
    bool get_version(uint32_t& version, size_t height, const header& header,
        size_t header_height, bool candidate) const;
    bool get_timestamp(uint32_t& time, size_t height, const header& header,
        size_t header_height, bool candidate) const;
    bool get_block_hash(system::hash_digest& hash, size_t height,
        const header& header, size_t header_height, bool candidate) const;

    bool populate_all(data& data, const header& header, size_t header_height,
        bool candidate) const;

    bool populate_bits(data& data, const map& map, const header& header,
        size_t header_height, bool candidate) const;
    bool populate_versions(data& data, const map& map, const header& header,
        size_t header_height, bool candidate) const;
    bool populate_timestamps(data& data, const map& map, const header& header,
        size_t header_height, bool candidate) const;
    bool populate_bip9_bit0(data& data, const map& map, const header& header,
        size_t header_height, bool candidate) const;
    bool populate_bip9_bit1(data& data, const map& map, const header& header,
        size_t header_height, bool candidate) const;

    // These are thread safe.
    const uint32_t forks_;
    const uint32_t stale_seconds_;
    const system::config::checkpoint::list checkpoints_;
    const system::settings& bitcoin_settings_;

    // This is used in a thread safe manner, as headers are never changed.
    const fast_chain& fast_chain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
