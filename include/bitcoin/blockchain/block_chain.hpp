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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_info.hpp>

namespace libbitcoin {
namespace blockchain {

/**
 * An interface to a blockchain backend.
 */
class BCB_API block_chain
{
public:
    /// Use "kind" for union differentiation.
    enum class point_kind : uint32_t
    {
        output = 0,
        spend = 1
    };

    struct history_row
    {
        /// Is this an output or spend
        point_kind kind;

        /// Input or output point.
        chain::point point;

        /// Block height of the transaction.
        uint64_t height;

        union
        {
            /// If output, then satoshis value of output.
            uint64_t value;

            /// If spend, then checksum hash of previous output point
            /// To match up this row with the output, recompute the
            /// checksum from the output row with spend_checksum(row.point)
            uint64_t previous_checksum;
        };
    };

    struct stealth_row
    {
        hash_digest ephemkey;
        short_hash address;
        hash_digest transaction_hash;
    };

    typedef std::vector<history_row> history;
    typedef std::vector<stealth_row> stealth;

    typedef std::shared_ptr<chain::block> ptr;
    typedef std::vector<ptr> list;
    
    typedef handle0 block_import_handler;
    typedef handle0 result_handler;
    typedef handle1<std::shared_ptr<chain::block>> block_fetch_handler;
    typedef handle1<hash_list> missing_block_hashes_fetch_handler;
    typedef handle1<chain::input_point> spend_fetch_handler;
    typedef handle1<chain::header> block_header_fetch_handler;
    typedef handle1<chain::transaction> transaction_fetch_handler;
    typedef handle1<message::block_locator> block_locator_fetch_handler;
    typedef handle1<uint64_t> last_height_fetch_handler;
    typedef handle1<uint64_t> block_height_fetch_handler;
    typedef handle1<hash_list> transaction_hashes_fetch_handler;
    typedef handle1<history> history_fetch_handler;
    typedef handle1<stealth> stealth_fetch_handler;
    typedef handle2<uint64_t, uint64_t> transaction_index_fetch_handler;
    typedef handle1<block_info> store_block_handler;
    typedef std::function<bool(const std::error_code&, uint64_t, const list&,
        const list&)> reorganize_handler;

    /// Create checksum so spend can be matched with corresponding
    /// output point without needing the whole previous outpoint.
    static uint64_t spend_checksum(chain::output_point outpoint);

    virtual void start(result_handler handler) = 0;
    virtual void stop(result_handler handler) = 0;
    virtual void stop() = 0;

    virtual void store(const chain::block& block,
        store_block_handler handle_store) = 0;

    virtual void import(const chain::block& import_block,
        block_import_handler handle_import) = 0;

    virtual void fetch_block_header(uint64_t height,
        block_header_fetch_handler handle_fetch) = 0;

    virtual void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handle_fetch) = 0;

    virtual void fetch_missing_block_hashes(const hash_list& hashes,
        missing_block_hashes_fetch_handler handle_fetch) = 0;

    virtual void fetch_block_transaction_hashes(const hash_digest& hash,
        transaction_hashes_fetch_handler handle_fetch) = 0;

    virtual void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handle_fetch) = 0;

    virtual void fetch_last_height(last_height_fetch_handler handle_fetch) = 0;

    virtual void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handle_fetch) = 0;

    virtual void fetch_transaction_index(const hash_digest& hash,
        transaction_index_fetch_handler handle_fetch) = 0;

    virtual void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handle_fetch) = 0;

    virtual void fetch_history(const wallet::payment_address& address,
        history_fetch_handler handle_fetch, const uint64_t limit=0,
        const uint64_t from_height=0) = 0;

    virtual void fetch_stealth(const binary_type& filter,
        stealth_fetch_handler handle_fetch, uint64_t from_height=0) = 0;

    virtual void subscribe_reorganize(reorganize_handler handler) = 0;

private:
    static uint64_t remainder_fast(const hash_digest& value,
        const uint64_t divisor);
};

} // namespace blockchain
} // namespace libbitcoin

#endif
