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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_BLOCK_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_BLOCK_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is NOT thread safe.
class BCB_API populate_block
{
public:
    typedef handle0 result_handler;

    populate_block(threadpool& priority_pool, const fast_chain& chain,
        const settings& settings);

    /// Stop the population (thread safe).
    void stop();

    /// Populate chain state for the block at index.
    void populate_chain_state(fork::const_ptr fork, size_t index) const;

    /// Populate block validation state for the block at index.
    void populate_block_state(fork::const_ptr fork, size_t index,
        result_handler handler) const;

protected:
    bool stopped() const;

private:
    // Aliases for this header.
    typedef fork::const_ptr fork_ptr;
    typedef chain::output_point point;
    typedef chain::chain_state::map map;
    typedef chain::chain_state::data data;

    // Chain State
    bool populate_bits(data& data, const map& map, fork_ptr fork) const;
    bool populate_versions(data& data, const map& map, fork_ptr fork) const;
    bool populate_timestamps(data& data, const map& map, fork_ptr fork) const;
    bool populate_checkpoint(data& data, const map& map, fork_ptr fork) const;
    bool get_bits(uint32_t& out_bits, size_t height, fork_ptr fork) const;
    bool get_version(uint32_t& out_version, size_t height, fork_ptr fork) const;
    bool get_timestamp(uint32_t& out_timestamp, size_t height,
        fork_ptr fork) const;
    bool get_block_hash(hash_digest& out_hash, size_t height,
        fork_ptr fork) const;

    // Block State
    void populate_coinbase(block_const_ptr block) const;
    void populate_transaction(size_t fork_height,
        const chain::transaction& tx) const;
    void populate_transaction(fork_ptr fork, size_t index,
        const chain::transaction& tx) const;
    void populate_inputs(fork::const_ptr fork, size_t index, size_t bucket,
        result_handler handler) const;
    void populate_prevout(size_t fork_height, const point& outpoint) const;
    void populate_prevout(fork_ptr fork, size_t index,
        const point& outpoint) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const size_t buckets_;
    const uint32_t configured_forks_;
    const config::checkpoint::list checkpoints_;
    mutable dispatcher dispatch_;

    // This is protected by caller not invoking populate concurrently.
    const fast_chain& fast_chain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
