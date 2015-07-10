/**
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoints.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace chain {

class BCB_API validate_block
{
public:
    std::error_code check_block();
    std::error_code accept_block();
    std::error_code connect_block();

    static bool check_proof_of_work(hash_digest hash, uint32_t bits);

protected:
    validate_block(size_t height, const block_type& current_block,
        const checkpoints& checkpoints);

    virtual uint32_t previous_block_bits() = 0;
    virtual uint64_t actual_timespan(size_t interval) = 0;
    virtual uint64_t median_time_past() = 0;
    virtual bool transaction_exists(const hash_digest& tx_hash) = 0;
    virtual bool is_output_spent(const output_point& outpoint) = 0;

    // These have optional implementations that can be overriden
    virtual bool validate_inputs(const transaction_type& tx,
        size_t index_in_parent, uint64_t& value_in, size_t& total_sigops);
    virtual bool connect_input(size_t index_in_parent,
        const transaction_type& current_tx, size_t input_index,
        uint64_t& value_in, size_t& total_sigops);
    virtual bool fetch_transaction(transaction_type& tx,
        size_t& previous_height, const hash_digest& tx_hash) = 0;
    virtual bool is_output_spent(const output_point& previous_output,
        size_t index_in_parent, size_t input_index) = 0;
    virtual block_header_type fetch_block(size_t fetch_height) = 0;

private:
    size_t legacy_sigops_count();

    // accept_block()
    uint32_t work_required();
    bool coinbase_height_match();

    // connect_block()
    bool not_duplicate_or_spent(const transaction_type& tx);

    const size_t height_;
    const block_type& current_block_;
    const checkpoints& checkpoints_;
};

} // namespace chain
} // namespace libbitcoin

#endif

