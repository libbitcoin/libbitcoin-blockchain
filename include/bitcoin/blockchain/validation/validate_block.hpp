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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_BLOCK_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_BLOCK_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/simple_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
class BCB_API validate_block
{
public:
    typedef handle0 result_handler;
    typedef config::checkpoint::list checkpoints;
    typedef chain::chain_state::versions versions;
    typedef std::function<bool()> stopped_callback;

    validate_block(threadpool& pool, const simple_chain& chain,
        const settings& settings);

    void stop();

    /// These are thread safe as long as reset is not called concurrently.
    code check(block_const_ptr block) const;
    code accept(block_const_ptr block) const;
    void connect(block_const_ptr block, result_handler handler) const;

    /// Call before invoking accept/connect for a given height/block.
    void populate(fork::const_ptr, size_t index, result_handler handler);

protected:
    bool stopped() const;

    void connect_input(const chain::transaction& tx,
        size_t input_index, result_handler handler) const;
    code validate_input(const chain::transaction& tx,
        size_t input_index) const;
    void handle_connect(const code& ec, block_const_ptr block,
        asio::time_point start_time, result_handler handler) const;

private:
    static code verify_script(const chain::transaction& tx,
        uint32_t input_index, uint32_t flags, bool use_libconsensus);

    void populate_block(fork::const_ptr fork, size_t index,
        result_handler handler);
    void populate_chain_state(fork::const_ptr fork, size_t index,
        result_handler handler);

    void populate_coinbase(block_const_ptr block) const;

    void populate_transaction(const chain::transaction& tx) const;
    void populate_transaction(fork::const_ptr fork, size_t index,
        const chain::transaction& tx) const;

    bool populate_spent(size_t fork_height,
        const chain::output_point& outpoint) const;
    void populate_spent(fork::const_ptr fork, size_t index,
        const chain::output_point& outpoint) const;

    void populate_prevout(size_t fork_height,
        const chain::output_point& outpoint) const;
    void populate_prevout(fork::const_ptr fork, size_t index,
        const chain::output_point& outpoint) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const simple_chain& chain_;
    const bool use_libconsensus_;
    const bool use_testnet_rules_;
    const config::checkpoint::list checkpoints_;
    mutable dispatcher dispatch_;

    // These are not thread safe.
    chain::chain_state chain_state_;
    versions history_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
