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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_BLOCK_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_BLOCK_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/populate/populate_block.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API validate_block
{
public:
    typedef handle0 result_handler;

    validate_block(dispatcher& dispatch, const fast_chain& chain,
        const settings& settings);

    void start();
    void stop();

    void check(block_const_ptr block, result_handler handler) const;
    void accept(block_const_ptr block, result_handler handler) const;
    void connect(block_const_ptr block, result_handler handler) const;

protected:
    bool stopped() const;
    float hit_rate() const;

private:
    typedef std::atomic<size_t> atomic_counter;
    typedef std::shared_ptr<atomic_counter> atomic_counter_ptr;

    static void dump(const code& ec, const chain::transaction& tx,
        uint32_t input_index, uint32_t forks, size_t height,
        bool use_libconsensus);

    ////void check_block(block_const_ptr block, size_t bucket, size_t buckets,
    ////    result_handler handler) const;
    ////void handle_checked(const code& ec, block_const_ptr block,
    ////    result_handler handler) const;
    void handle_populated(const code& ec, block_const_ptr block,
        result_handler handler) const;
    void accept_transactions(block_const_ptr block, size_t bucket,
        size_t buckets, atomic_counter_ptr sigops, bool bip16, bool bip141,
        result_handler handler) const;
    void handle_accepted(const code& ec, block_const_ptr block,
        atomic_counter_ptr sigops, bool bip141, result_handler handler) const;
    void connect_inputs(block_const_ptr block, size_t bucket,
        size_t buckets, result_handler handler) const;
    void handle_connected(const code& ec, block_const_ptr block,
        result_handler handler) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const bool use_libconsensus_;
    dispatcher& priority_dispatch_;
    mutable atomic_counter hits_;
    mutable atomic_counter queries_;
    populate_block block_populator_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
