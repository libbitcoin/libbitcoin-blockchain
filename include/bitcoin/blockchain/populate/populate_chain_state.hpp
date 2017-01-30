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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_CHAIN_STATE_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_CHAIN_STATE_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is NOT thread safe.
class BCB_API populate_chain_state
{
public:
    populate_chain_state(const fast_chain& chain, const settings& settings);

    /// Populate chain state for the tx pool (start).
    chain::chain_state::ptr populate() const;

    /// Populate chain state for the top block in the branch (try).
    chain::chain_state::ptr populate(chain::chain_state::ptr pool,
        branch::const_ptr branch) const;

    /// Populate pool state from the top block (organized).
    chain::chain_state::ptr populate(chain::chain_state::ptr top) const;

private:
    typedef branch::const_ptr branch_ptr;
    typedef chain::chain_state::map map;
    typedef chain::chain_state::data data;

    bool populate_all(data& data, branch_ptr branch) const;
    bool populate_bits(data& data, const map& map, branch_ptr branch) const;
    bool populate_versions(data& data, const map& map, branch_ptr branch) const;
    bool populate_timestamps(data& data, const map& map, branch_ptr branch) const;
    bool populate_checkpoint(data& data, const map& map, branch_ptr branch) const;

    bool get_bits(uint32_t& out_bits, size_t height, branch_ptr branch) const;
    bool get_version(uint32_t& out_version, size_t height, branch_ptr branch) const;
    bool get_timestamp(uint32_t& out_timestamp, size_t height,
        branch_ptr branch) const;
    bool get_block_hash(hash_digest& out_hash, size_t height,
        branch_ptr branch) const;

    // These are thread safe.
    const uint32_t block_version_;
    const uint32_t configured_forks_;
    const config::checkpoint::list checkpoints_;

    // The store is protected by caller not invoking populate concurrently.
    const fast_chain& fast_chain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
