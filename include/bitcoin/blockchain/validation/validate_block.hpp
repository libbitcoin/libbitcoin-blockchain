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

    validate_block(threadpool& pool, bool testnet, bool libconsensus,
        const checkpoints& checkpoints, const simple_chain& chain);

    void stop();

    /// These are thread safe as long as reset is not called concurrently.
    code check(block_const_ptr block) const;
    code accept(block_const_ptr block) const;
    void connect(block_const_ptr block, result_handler handler) const;

    /// Call each before invoking accept/connect.
    code reset(size_t height);
    code populate(block_const_ptr block) const;

protected:
    bool stopped() const;

    void connect_input(const chain::transaction& tx,
        uint32_t input_index, result_handler handler) const;
    code validate_input(const chain::transaction& tx,
        uint32_t input_index) const;
    void handle_connect(const code& ec, block_const_ptr block,
        asio::time_point start_time, result_handler handler) const;

private:
    static code verify_script(const chain::transaction& tx,
        uint32_t input_index, uint32_t flags, bool use_libconsensus);

    // These are not thread safe.
    chain::chain_state chain_state_;
    versions history_;

    // These are thread safe.
    std::atomic<bool> stopped_;
    mutable dispatcher dispatch_;
    const simple_chain& chain_;
    const bool use_libconsensus_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
