/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This interface is thread safe.
/// A high level interface for encapsulation of the blockchain database.
/// Implementations are expected to be thread safe.
class BCB_API safe_chain
{
public:
    typedef system::handle0 result_handler;

    /// Object fetch handlers.
    typedef system::handle1<size_t> last_height_fetch_handler;
    typedef system::handle1<size_t> block_height_fetch_handler;
    typedef system::handle1<system::chain::output> output_fetch_handler;
    typedef system::handle1<system::chain::input_point> spend_fetch_handler;
    typedef system::handle1<system::chain::payment_record::list>
        history_fetch_handler;
    typedef system::handle1<system::chain::stealth_record::list>
        stealth_fetch_handler;
    typedef system::handle2<size_t, size_t> transaction_index_fetch_handler;

    // Smart pointer parameters must not be passed by reference.
    typedef std::function<void(const system::code&, system::block_const_ptr,
        size_t)> block_fetch_handler;
    typedef std::function<void(const system::code&, system::merkle_block_ptr,
        size_t)> merkle_block_fetch_handler;
    typedef std::function<void(const system::code&, system::compact_block_ptr,
        size_t)> compact_block_fetch_handler;
    typedef std::function<void(const system::code&,
        system::header_ptr, size_t)> block_header_fetch_handler;
    typedef std::function<void(const system::code&,
        system::compact_filter_ptr, size_t)> compact_filter_fetch_handler;
    typedef std::function<void(const system::code&,
        system::compact_filter_checkpoint_ptr)>
            compact_filter_checkpoint_fetch_handler;
    typedef std::function<void(const system::code&,
        system::compact_filter_headers_ptr)>
            compact_filter_headers_fetch_handler;
    typedef std::function<void(const system::code&,
        system::transaction_const_ptr, size_t, size_t)>
            transaction_fetch_handler;
    typedef std::function<void(const system::code&, system::headers_ptr)>
        locator_block_headers_fetch_handler;
    typedef std::function<void(const system::code&, system::get_blocks_ptr)>
        block_locator_fetch_handler;
    typedef std::function<void(const system::code&, system::get_headers_ptr)>
        header_locator_fetch_handler;
    typedef std::function<void(const system::code&, system::inventory_ptr)>
        inventory_fetch_handler;

    /// Subscription handlers.
    typedef std::function<bool(system::code, size_t,
        system::header_const_ptr_list_const_ptr,
        system::header_const_ptr_list_const_ptr)> header_handler;
    typedef std::function<bool(system::code, size_t,
        system::block_const_ptr_list_const_ptr,
        system::block_const_ptr_list_const_ptr)> block_handler;
    typedef std::function<bool(system::code, system::transaction_const_ptr)>
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

    virtual void fetch_block(const system::hash_digest& hash, bool witness,
        block_fetch_handler handler) const = 0;

    virtual void fetch_block_header(size_t height,
        block_header_fetch_handler handler) const = 0;

    virtual void fetch_block_header(const system::hash_digest& hash,
        block_header_fetch_handler handler) const = 0;

    virtual void fetch_compact_filter(uint8_t filter_type, size_t height,
        compact_filter_fetch_handler handler) const = 0;

    virtual void fetch_compact_filter(uint8_t filter_type,
        const system::hash_digest& hash,
        compact_filter_fetch_handler handler) const = 0;

    virtual void fetch_compact_filter_headers(uint8_t filter_type,
        size_t start_height, const system::hash_digest& stop_hash,
        compact_filter_headers_fetch_handler handler) const = 0;

    virtual void fetch_compact_filter_headers(uint8_t filter_type,
        size_t start_height, size_t stop_height,
        compact_filter_headers_fetch_handler handler) const = 0;

    virtual void fetch_compact_filter_checkpoint(uint8_t filter_type,
        const system::hash_digest& stop_hash,
        compact_filter_checkpoint_fetch_handler handler) const = 0;

    virtual void fetch_merkle_block(size_t height,
        merkle_block_fetch_handler handler) const = 0;

    virtual void fetch_merkle_block(const system::hash_digest& hash,
        merkle_block_fetch_handler handler) const = 0;

    virtual void fetch_compact_block(size_t height,
        compact_block_fetch_handler handler) const = 0;

    virtual void fetch_compact_block(const system::hash_digest& hash,
        compact_block_fetch_handler handler) const = 0;

    virtual void fetch_block_height(const system::hash_digest& hash,
        block_height_fetch_handler handler) const = 0;

    virtual void fetch_last_height(
        last_height_fetch_handler handler) const = 0;

    virtual void fetch_transaction(const system::hash_digest& hash,
        bool require_confirmed, bool witness,
        transaction_fetch_handler handler) const = 0;

    virtual void fetch_transaction_position(const system::hash_digest& hash,
        bool require_confirmed,
        transaction_index_fetch_handler handler) const = 0;

    virtual void fetch_locator_block_hashes(
        system::get_blocks_const_ptr locator,
        const system::hash_digest& threshold, size_t limit,
        inventory_fetch_handler handler) const = 0;

    virtual void fetch_locator_block_headers(
        system::get_headers_const_ptr locator,
        const system::hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const = 0;

    ////// TODO: must be branch-relative.
    ////virtual void fetch_block_locator(const chain::block::indexes& heights,
    ////    block_locator_fetch_handler handler) const = 0;

    // TODO: must be branch-relative.
    virtual void fetch_header_locator(
        const system::chain::block::indexes& heights,
        header_locator_fetch_handler handler) const = 0;

    // Server Queries.
    //-------------------------------------------------------------------------
    // Confirmed heights only.

    virtual void fetch_spend(const system::chain::output_point& outpoint,
        spend_fetch_handler handler) const = 0;

    virtual void fetch_history(const system::hash_digest& script_hash,
        size_t limit, size_t from_height,
        history_fetch_handler handler) const = 0;

    virtual void fetch_stealth(const system::binary& filter,
        size_t from_height, stealth_fetch_handler handler) const = 0;

    // Transaction Pool.
    //-------------------------------------------------------------------------

    virtual void fetch_template(merkle_block_fetch_handler handler) const = 0;
    virtual void fetch_mempool(size_t count_limit, uint64_t minimum_fee,
        inventory_fetch_handler handler) const = 0;

    // Filters.
    //-------------------------------------------------------------------------

    virtual void filter_blocks(system::get_data_ptr message,
        result_handler handler) const = 0;

    virtual void filter_transactions(system::get_data_ptr message,
        result_handler handler) const = 0;

    // Subscribers.
    //-------------------------------------------------------------------------

    virtual void subscribe_blocks(block_handler&& handler) = 0;
    virtual void subscribe_headers(header_handler&& handler) = 0;
    virtual void subscribe_transactions(transaction_handler&& handler) = 0;
    virtual void unsubscribe() = 0;

    // Organizers.
    //-------------------------------------------------------------------------

    virtual void organize(system::header_const_ptr header,
        result_handler handler) = 0;
    virtual void organize(system::transaction_const_ptr tx,
        result_handler handler) = 0;
    virtual system::code organize(system::block_const_ptr block,
        size_t height) = 0;

    // Properties
    // ------------------------------------------------------------------------

    virtual bool is_blocks_stale() const = 0;
    virtual bool is_candidates_stale() const = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
