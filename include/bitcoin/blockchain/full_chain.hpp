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
#ifndef LIBBITCOIN_BLOCKCHAIN_FULL_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_FULL_CHAIN_HPP

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
class BCB_API full_chain
{
public:
    typedef handle0 result_handler;
    typedef handle0 block_import_handler;
    typedef handle1<uint64_t> block_store_handler;
    typedef handle1<hash_list> block_locator_fetch_handler;
    typedef handle1<hash_list> locator_block_hashes_fetch_handler;
    typedef handle1<chain::header::list> locator_block_headers_fetch_handler;
    typedef handle1<hash_list> transaction_hashes_fetch_handler;
    typedef handle1<uint64_t> block_height_fetch_handler;
    typedef handle1<uint64_t> last_height_fetch_handler;
    typedef handle1<chain::input_point> spend_fetch_handler;
    typedef handle1<chain::history_compact::list> history_fetch_handler;
    typedef handle1<chain::stealth_compact::list> stealth_fetch_handler;
    typedef handle2<uint64_t, uint64_t> transaction_index_fetch_handler;

    // Fetch non-const so that values can be safely swapped/moved.
    // Smart pointer parameters must not be passed by reference.
    typedef std::function<void(const code&, block_ptr, uint64_t)>
        block_fetch_handler;
    typedef std::function<void(const code&, header_ptr, uint64_t)>
        block_header_fetch_handler;
    typedef std::function<void(const code&, transaction_ptr, uint64_t)>
        transaction_fetch_handler;

    typedef std::function<bool(const code&, size_t,
        const block_const_ptr_list&, const block_const_ptr_list&)>
        reorganize_handler;

    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool close() = 0;

    virtual void store(block_const_ptr block, block_store_handler handler) = 0;

    virtual void fetch_block(uint64_t height,
        block_fetch_handler handler) const = 0;
    virtual void fetch_block(const hash_digest& hash,
        block_fetch_handler handler) const = 0;

    virtual void fetch_block_header(uint64_t height,
        block_header_fetch_handler handler) const = 0;
    virtual void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler) const = 0;

    virtual void fetch_block_transaction_hashes(uint64_t height,
        transaction_hashes_fetch_handler handler) const = 0;
    virtual void fetch_block_transaction_hashes(const hash_digest& hash,
        transaction_hashes_fetch_handler handler) const = 0;

    virtual void fetch_block_locator(
        block_locator_fetch_handler handler) const = 0;

    virtual void fetch_locator_block_hashes(get_blocks_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_hashes_fetch_handler handler) const = 0;

    virtual void fetch_locator_block_headers(get_headers_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const = 0;

    virtual void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler) const = 0;

    virtual void fetch_last_height(
        last_height_fetch_handler handler) const = 0;

    virtual void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handler) const = 0;

    virtual void fetch_transaction_index(const hash_digest& hash,
        transaction_index_fetch_handler handler) const = 0;

    virtual void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler) const = 0;

    virtual void fetch_history(const wallet::payment_address& address,
        uint64_t limit, uint64_t from_height,
        history_fetch_handler handler) const = 0;

    virtual void fetch_stealth(const binary& filter, uint64_t from_height,
        stealth_fetch_handler handler) const = 0;

    virtual void filter_blocks(get_data_ptr message,
        result_handler handler) const = 0;

    virtual void filter_orphans(get_data_ptr message,
        result_handler handler) const = 0;

    virtual void filter_transactions(get_data_ptr message,
        result_handler handler) const = 0;

    virtual void subscribe_reorganize(reorganize_handler handler) = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
