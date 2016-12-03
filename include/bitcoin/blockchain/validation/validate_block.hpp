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
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/populate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is NOT thread safe.
class BCB_API validate_block
{
public:
    typedef handle0 result_handler;

    static void report(block_const_ptr block,
        const asio::time_point& start_time, const std::string& token);

    validate_block(threadpool& priority_pool, const fast_chain& chain,
        const settings& settings);

    void stop();

    code check(block_const_ptr block) const;
    void accept(fork::const_ptr fork, size_t index,
        result_handler handler) const;
    void connect(fork::const_ptr fork, size_t index,
        result_handler handler) const;

protected:
    bool stopped() const;

private:
    typedef std::atomic<size_t> atomic_counter;
    typedef std::shared_ptr<atomic_counter> atomic_counter_ptr;

    static void dump(const code& ec, const chain::transaction& tx,
        uint32_t input_index, uint32_t forks, size_t height,
        bool use_libconsensus);

    void handle_populated(const code& ec, block_const_ptr block,
        const asio::time_point& start_time, result_handler handler) const;

    void accept_transactions(block_const_ptr block, size_t bucket,
        atomic_counter_ptr sigops, bool bip16, result_handler handler) const;

    void handle_accepted(const code& ec, block_const_ptr block,
        const asio::time_point& start_time, atomic_counter_ptr sigops,
        result_handler handler) const;

    void connect_inputs(block_const_ptr block, size_t bucket,
        result_handler handler) const;

    void handle_connected(const code& ec, block_const_ptr block,
        const asio::time_point& start_time, result_handler handler) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const size_t buckets_;
    const bool use_libconsensus_;
    mutable dispatcher priority_dispatch_;

    // Caller must not invoke accept/connect concurrently.
    populate_block populator_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
