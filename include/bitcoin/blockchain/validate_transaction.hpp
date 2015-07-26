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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_TRANSACTION_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_TRANSACTION_HPP

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>

namespace libbitcoin {
namespace chain {

/**
 * If you're looking to validate a transaction, then use the simpler
 * transaction_pool::validate() method instead.
 */
class BCB_API validate_transaction
  : public std::enable_shared_from_this<validate_transaction>
{
public:
    typedef std::function<
        void (const std::error_code&, const index_list&)> validate_handler;

    validate_transaction(blockchain& chain, const transaction_type& tx,
        const pool_buffer& pool, async_strand& strand);
    void start(validate_handler handle_validate);

    static std::error_code check_transaction(const transaction_type& tx);
    static bool connect_input(const transaction_type& tx, size_t current_input,
        const transaction_type& previous_tx, size_t parent_height,
        size_t last_block_height, uint64_t& value_in);
    static bool tally_fees(const transaction_type& tx, uint64_t value_in,
        uint64_t& fees);
    static bool validate_consensus(const script_type& prevout_script,
        const transaction_type& current_tx, size_t input_index,
        const block_header_type& header, const size_t height);

private:
    std::error_code basic_checks() const;
    bool is_standard() const;
    const transaction_type* fetch(const hash_digest& tx_hash) const;

    void handle_duplicate_check(const std::error_code& ec);
    bool is_spent(const output_point& outpoint) const;

    // Used for checking coinbase maturity
    void set_last_height(const std::error_code& ec, size_t last_height);

    // Begin looping through the inputs, fetching the previous tx
    void next_previous_transaction();
    void previous_tx_index(const std::error_code& ec, size_t parent_height);

    // If previous_tx_index didn't find it then check in pool instead
    void search_pool_previous_tx();
    void handle_previous_tx(const std::error_code& ec,
        const transaction_type& previous_tx, size_t parent_height);

    // After running connect_input, we check whether this validated previous
    // output was not already spent by another input in the blockchain.
    // is_spent() earlier already checked in the pool.
    void check_double_spend(const std::error_code& ec);
    void check_fees();

    async_strand& strand_;
    blockchain& blockchain_;
    const transaction_type tx_;
    const hash_digest tx_hash_;
    const pool_buffer& pool_;
    size_t last_block_height_;
    uint64_t value_in_;
    size_t current_input_;
    index_list unconfirmed_;
    validate_handler handle_validate_;
};

// TODO: define in validate_transaction (compat break).
typedef std::shared_ptr<validate_transaction> validate_transaction_ptr;

} // namespace chain
} // namespace libbitcoin

#endif

