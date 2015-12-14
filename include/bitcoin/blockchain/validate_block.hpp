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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <boost/date_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoint.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace chain {

class BCB_API validate_block
{
public:
    std::error_code check_block() const;
    std::error_code accept_block() const;
    std::error_code connect_block() const;

    /// Required to call before calling accept_block or connect_block.
    void initialize_context();

protected:
    typedef std::vector<uint8_t> versions;
    typedef std::function<bool()> stopped_callback;

    validate_block(size_t height, const block_type& block,
        const config::checkpoint::list& checks,
        stopped_callback stop_callback=nullptr);

    virtual uint64_t actual_timespan(size_t interval) const = 0;
    virtual block_header_type fetch_block(size_t fetch_height) const = 0;
    virtual bool fetch_transaction(transaction_type& tx,
        size_t& previous_height, const hash_digest& tx_hash) const = 0;
    virtual bool is_output_spent(const output_point& outpoint) const = 0;
    virtual bool is_output_spent(const output_point& previous_output,
        size_t index_in_parent, size_t input_index) const = 0;
    virtual uint64_t median_time_past() const = 0;
    virtual uint32_t previous_block_bits() const = 0;
    virtual versions preceding_block_versions(size_t count) const = 0;
    virtual bool transaction_exists(const hash_digest& tx_hash) const = 0;

    // These have default implementations that can be overriden.
    virtual bool connect_input(size_t index_in_parent,
        const transaction_type& current_tx, size_t input_index,
        uint64_t& value_in, size_t& total_sigops) const;
    virtual bool validate_inputs(const transaction_type& tx,
        size_t index_in_parent, uint64_t& value_in,
        size_t& total_sigops) const;

    // These are protected virtual for testability.
    virtual boost::posix_time::ptime current_time() const;
    virtual bool stopped() const;
    virtual bool is_valid_version() const;
    virtual bool is_active(script_context flag) const;
    virtual bool is_spent_duplicate(const transaction_type& tx) const;
    virtual bool is_valid_time_stamp(uint32_t timestamp) const;
    virtual uint32_t work_required() const;

    static bool is_distinct_tx_set(const transaction_list& txs);
    static bool is_valid_proof_of_work(hash_digest hash, uint32_t bits);
    static bool is_valid_coinbase_height(size_t height,
        const block_type& block);
    static size_t legacy_sigops_count(const transaction_type& tx);
    static size_t legacy_sigops_count(const transaction_list& txs);

private:
    const size_t height_;
    uint32_t activations_;
    uint32_t minimum_version_;
    const block_type& current_block_;
    const config::checkpoint::list& checkpoints_;
    const stopped_callback stop_callback_;
};

} // namespace chain
} // namespace libbitcoin

#endif

