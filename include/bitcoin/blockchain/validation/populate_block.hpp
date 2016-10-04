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

    void stop();

    /// Populate all validation state for block[index] and to on the block.
    void populate(fork::const_ptr fork, size_t index,
        result_handler handler) const;

protected:
    bool stopped() const;

private:
    // Aliases for this header.
    typedef fork::const_ptr fork_ptr;
    typedef chain::output_point point;
    typedef chain::chain_state::map map;
    typedef chain::chain_state::data data;
    typedef chain::transaction::sets_const_ptr sets_ptr;
    
    // Logging helper.
    static void report(block_const_ptr block, asio::time_point start_time,
        const std::string& token);

    // Input Sets
    void populate_input_sets(fork::const_ptr fork, size_t index) const;

    // Chain State
    void populate_chain_state(fork_ptr fork, size_t index) const;
    bool populate_bits(data& data, const map& map, fork_ptr fork) const;
    bool populate_versions(data& data, const map& map, fork_ptr fork) const;
    bool populate_timestamps(data& data, const map& map, fork_ptr fork) const;
    bool get_bits(uint32_t& out_bits, size_t height, fork_ptr fork) const;
    bool get_version(uint32_t& out_version, size_t height, fork_ptr fork) const;
    bool get_timestamp(uint32_t& out_timestamp, size_t height,
        fork_ptr fork) const;

    // Previous Outputs
    void populate_transactions(fork_ptr fork, size_t index,
        result_handler handler) const;
    void populate_coinbase(block_const_ptr block) const;
    void populate_transaction(const chain::transaction& tx) const;
    void populate_transaction(fork_ptr fork, size_t index,
        const chain::transaction& tx) const;

    void populate_inputs(fork_ptr fork, size_t index, sets_ptr input_sets,
        size_t sets_index, result_handler handler) const;

    bool populate_spent(size_t fork_height, const point& outpoint) const;
    void populate_spent(fork_ptr fork, size_t index, const point& outpoint) const;
    void populate_prevout(size_t fork_height, const point& outpoint) const;
    void populate_prevout(fork_ptr fork, size_t index, const point& outpoint) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const size_t priority_threads_;
    const bool use_testnet_rules_;
    const config::checkpoint::list checkpoints_;
    mutable dispatcher dispatch_;

    // This is protected by caller not invoking populate concurrently.
    const fast_chain& fast_chain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
