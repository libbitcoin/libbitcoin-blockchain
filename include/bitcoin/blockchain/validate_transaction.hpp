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
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block_chain.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

class transaction_pool;

/// This class is not thread safe.
/// This is a utility for transaction_pool::validate and validate_block.
class BCB_API validate_transaction
  : public enable_shared_from_base<validate_transaction>
{
public:
    typedef std::shared_ptr<validate_transaction> ptr;
    typedef message::transaction_message::ptr transaction_ptr;
    typedef chain::point::indexes indexes;
    typedef handle0 result_handler;
    typedef handle1<indexes> validate_handler;

    // Used for tx and block validation.
    //-------------------------------------------------------------------------

    // pointers (mempool)

    /// Expensive/final checks for both block and mempool transactions.
    static code check_input(transaction_ptr tx, uint32_t input_index,
        const chain::transaction& previous_tx, size_t previous_tx_height,
        size_t last_height, uint32_t flags, uint64_t& out_value);

    /// Expensive/final checks for both block and mempool transactions.
    static code check_script(transaction_ptr tx, uint32_t input_index,
        const chain::script& prevout_script, uint32_t flags);

    // references (block)

    /// Expensive/final checks for both block and mempool transactions.
    static code check_input(const chain::transaction& tx, uint32_t input_index,
        const chain::transaction& previous_tx, size_t previous_tx_height,
        size_t last_height, uint32_t flags, uint64_t& out_value);

    /// Expensive/final checks for both block and mempool transactions.
    static code check_script(const chain::transaction& tx,
        uint32_t input_index, const chain::script& prevout_script,
        uint32_t flags);

    // Used for memory pool transaction validation.
    //-------------------------------------------------------------------------

    validate_transaction(block_chain& chain, const transaction_pool& pool,
        dispatcher& dispatch);

    void validate(transaction_ptr tx, validate_handler handler);

private:
    // Determine if there is another transaction with the same id.
    void handle_duplicate(const code& ec, uint64_t height,
        uint64_t index, transaction_ptr tx, validate_handler handler);

    // Get last height for potential use in coinbase output maturity test.
    void handle_last_height(const code& ec, size_t last_height,
        transaction_ptr tx, validate_handler handler);

    // Start of input->output sequence.
    void validate_input(transaction_ptr tx, uint32_t input_index,
        size_t last_height, validate_handler handler);

    // Determine if output is spent.
    void handle_double_spend(const code& ec, const chain::input_point&,
        transaction_ptr tx, uint32_t input_index, size_t last_height,
        validate_handler handler);

    // Find output (spent or otherwise) and save block height for maturity test.
    void handle_previous_tx(const code& ec,
        const chain::transaction& previous_tx, uint64_t previous_tx_height,
        transaction_ptr tx, uint32_t input_index, size_t last_height,
        validate_handler handler);

    // Join input threads.
    void handle_join(const code& ec, chain::point::indexes unconfirmed,
        transaction_ptr tx, validate_handler handler);

    block_chain& blockchain_;
    const transaction_pool& pool_;
    dispatcher& dispatch_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
