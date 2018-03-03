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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_CHAIN_STATE_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_CHAIN_STATE_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
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
    populate_chain_state(const fast_chain& chain, const settings& settings);

    /// Populate chain state for the top block|header.
    chain::chain_state::ptr populate(bool block_index) const;

    /// Populate chain state for the top of the given header branch.
    chain::chain_state::ptr populate(header_branch::const_ptr branch) const;

private:
    typedef chain::chain_state::map map;
    typedef chain::chain_state::data data;
    typedef header_branch::const_ptr headers;

    bool get_bits(uint32_t& bits, size_t height, headers branch,
        bool block) const;
    bool get_version(uint32_t& version, size_t height, headers branch,
        bool block) const;
    bool get_timestamp(uint32_t& time, size_t height, headers branch,
        bool block) const;
    bool get_block_hash(hash_digest& hash, size_t height, headers branch,
        bool block) const;

    bool populate_all(data& data, headers branch, bool block) const;
    bool populate_bits(data& data, const map& map, headers branch,
        bool block) const;
    bool populate_versions(data& data, const map& map, headers branch,
        bool block) const;
    bool populate_timestamps(data& data, const map& map, headers branch,
        bool block) const;
    bool populate_bip9_bit0(data& data, const map& map, headers branch,
        bool block) const;
    bool populate_bip9_bit1(data& data, const map& map, headers branch,
        bool block) const;

    // These are thread safe.
    const uint32_t forks_;
    const uint32_t stale_seconds_;
    const config::checkpoint::list checkpoints_;

    // Populate is guarded against concurrent callers but because it uses the fast
    // chain it must not be invoked during chain writes.
    const fast_chain& fast_chain_;
    mutable shared_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
