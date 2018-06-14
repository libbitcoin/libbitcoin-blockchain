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
#ifndef LIBBITCOIN_BLOCKCHAIN_SAFE_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_SAFE_CHAIN_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This interface is thread safe.
/// A high level interface for encapsulation of the blockchain database.
/// Implementations are expected to be thread safe.
class BCB_API safe_chain
{
public:
    typedef handle0 result_handler;

    /// Object fetch handlers.
    typedef handle1<size_t> last_height_fetch_handler;
    typedef handle1<size_t> block_height_fetch_handler;
    typedef handle1<chain::output> output_fetch_handler;
    typedef handle1<chain::input_point> spend_fetch_handler;
    typedef handle1<chain::payment_record::list> history_fetch_handler;
    typedef handle1<chain::stealth_record::list> stealth_fetch_handler;
    typedef handle2<size_t, size_t> transaction_index_fetch_handler;

    // Smart pointer parameters must not be passed by reference.
    typedef std::function<void(const code&, block_const_ptr, size_t)>
        block_fetch_handler;
    typedef std::function<void(const code&, merkle_block_ptr, size_t)>
        merkle_block_fetch_handler;
    typedef std::function<void(const code&, compact_block_ptr, size_t)>
        compact_block_fetch_handler;
    typedef std::function<void(const code&, header_ptr, size_t)>
        block_header_fetch_handler;
    typedef std::function<void(const code&, transaction_const_ptr, size_t,
        size_t)> transaction_fetch_handler;
    typedef std::function<void(const code&, headers_ptr)>
        locator_block_headers_fetch_handler;
    typedef std::function<void(const code&, get_blocks_ptr)>
        block_locator_fetch_handler;
    typedef std::function<void(const code&, get_headers_ptr)>
        header_locator_fetch_handler;
    typedef std::function<void(const code&, inventory_ptr)>
        inventory_fetch_handler;

    /// Subscription handlers.
    typedef std::function<bool(code, size_t, header_const_ptr_list_const_ptr,
        header_const_ptr_list_const_ptr)> header_handler;
    typedef std::function<bool(code, size_t, block_const_ptr_list_const_ptr,
        block_const_ptr_list_const_ptr)> block_handler;
    typedef std::function<bool(code, transaction_const_ptr)>
        transaction_handler;

    // Startup and shutdown.
    // ------------------------------------------------------------------------

    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool close() = 0;

    // Node Queries.
    // ------------------------------------------------------------------------

    virtual void fetch_block(size_t height, bool witness,
        block_fetch_handler handler) const = 0;

    virtual void fetch_block(const hash_digest& hash, bool witness,
        block_fetch_handler handler) const = 0;

    virtual void fetch_block_header(size_t height,
        block_header_fetch_handler handler) const = 0;

    virtual void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler) const = 0;

    virtual void fetch_merkle_block(size_t height,
        merkle_block_fetch_handler handler) const = 0;

    virtual void fetch_merkle_block(const hash_digest& hash,
        merkle_block_fetch_handler handler) const = 0;

    virtual void fetch_compact_block(size_t height,
        compact_block_fetch_handler handler) const = 0;

    virtual void fetch_compact_block(const hash_digest& hash,
        compact_block_fetch_handler handler) const = 0;

    virtual void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler) const = 0;

    virtual void fetch_last_height(
        last_height_fetch_handler handler) const = 0;

    virtual void fetch_transaction(const hash_digest& hash,
        bool require_confirmed, bool witness,
        transaction_fetch_handler handler) const = 0;

    virtual void fetch_transaction_position(const hash_digest& hash,
        bool require_confirmed,
        transaction_index_fetch_handler handler) const = 0;

    virtual void fetch_locator_block_hashes(get_blocks_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        inventory_fetch_handler handler) const = 0;

    virtual void fetch_locator_block_headers(get_headers_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const = 0;

    ////// TODO: must be branch-relative.
    ////virtual void fetch_block_locator(const chain::block::indexes& heights,
    ////    block_locator_fetch_handler handler) const = 0;

    // TODO: must be branch-relative.
    virtual void fetch_header_locator(const chain::block::indexes& heights,
        header_locator_fetch_handler handler) const = 0;

    // Server Queries.
    //-------------------------------------------------------------------------
    // Confirmed heights only.

    virtual void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler) const = 0;

    virtual void fetch_history(const short_hash& address_hash, size_t limit,
        size_t from_height, history_fetch_handler handler) const = 0;

    virtual void fetch_stealth(const binary& filter, size_t from_height,
        stealth_fetch_handler handler) const = 0;

    // Transaction Pool.
    //-------------------------------------------------------------------------

    virtual void fetch_template(merkle_block_fetch_handler handler) const = 0;
    virtual void fetch_mempool(size_t count_limit, uint64_t minimum_fee,
        inventory_fetch_handler handler) const = 0;

    // Filters.
    //-------------------------------------------------------------------------

    virtual void filter_blocks(get_data_ptr message,
        result_handler handler) const = 0;

    virtual void filter_transactions(get_data_ptr message,
        result_handler handler) const = 0;

    // Subscribers.
    //-------------------------------------------------------------------------

    virtual void subscribe_blocks(block_handler&& handler) = 0;
    virtual void subscribe_headers(header_handler&& handler) = 0;
    virtual void subscribe_transactions(transaction_handler&& handler) = 0;
    virtual void unsubscribe() = 0;

    // Organizers.
    //-------------------------------------------------------------------------

    virtual void organize(header_const_ptr header, result_handler handler) = 0;
    virtual void organize(transaction_const_ptr tx, result_handler handler) = 0;
    virtual code organize(block_const_ptr block, size_t height) = 0;

    // Properties
    // ------------------------------------------------------------------------

    virtual bool is_blocks_stale() const = 0;
    virtual bool is_candidates_stale() const = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
