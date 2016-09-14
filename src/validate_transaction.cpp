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
#include <bitcoin/blockchain/validate_transaction.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace chain;
using namespace std::placeholders;

#define NAME "validate_transaction"

// Maximum transaction size is set to max block size (1,000,000).
static constexpr uint32_t max_transaction_size = 1000000;

// Maximum transaction signature operations is set to block limit (20000).
static constexpr uint32_t max_transaction_sigops = max_transaction_size / 50;

// mempool check
validate_transaction::validate_transaction(block_chain& chain,
    const transaction_pool& pool, dispatcher& dispatch)
  : blockchain_(chain),
    pool_(pool),
    dispatch_(dispatch)
{
}

// Validate sequence.
//-----------------------------------------------------------------------------

void validate_transaction::validate(const chain::transaction& tx,
    validate_handler handler)
{
    validate(std::make_shared<message::transaction_message>(tx), handler);
}

void validate_transaction::validate(transaction_ptr tx,
    validate_handler handler)
{
    if (tx->is_coinbase())
    {
        // No coinbase in mempool.
        handler(error::coinbase_transaction, {}, 0);
        return;
    }

    // Basic checks that are independent of the mempool and the database.
    const auto error_code = check_transaction(tx);

    if (error_code)
    {
        handler(error_code, {}, 0);
        return;
    }

    // Check for a duplicate transaction identifier (hash).
    blockchain_.fetch_transaction(tx->hash(),
        dispatch_.unordered_delegate(&validate_transaction::handle_duplicate,
            shared_from_this(), _1, _2, tx, handler));
}

void validate_transaction::handle_duplicate(const code& ec,
    const transaction&, transaction_ptr tx, validate_handler handler)
{
    if (!ec)
    {
        // BUGBUG: overly restrictive, spent dups ok (BIP30).
        handler(error::duplicate, {}, 0);
        return;
    }

    if (ec != error::not_found)
    {
        handler(ec, {}, 0);
        return;
    }

    if (pool_.is_in_pool(tx->hash()))
    {
        // Without pool consistency this is impossible to enforce.
        handler(error::duplicate, {}, 0);
        return;
    }

    // Get chain height for determining coinbase maturity.
    blockchain_.fetch_last_height(
        dispatch_.unordered_delegate(&validate_transaction::handle_last_height,
            shared_from_this(), _1, _2, tx, handler));
}

void validate_transaction::handle_last_height(const code& ec,
    size_t last_height, transaction_ptr tx, validate_handler handler)
{
    if (ec)
    {
        handler(ec, {}, 0);
        return;
    }

    const validate_handler rejoin =
        std::bind(&validate_transaction::handle_join,
            shared_from_this(), _1, _2, _3, tx, handler);

    const validate_handler complete =
        synchronize(rejoin, tx->inputs.size(), NAME);

    // Asynchronously loop all inputs in transaction.
    for (uint32_t index = 0; index < tx->inputs.size(); ++index)
        dispatch_.concurrent(&validate_transaction::handle_input,
            shared_from_this(), tx, index, last_height, complete);
}

void validate_transaction::handle_input(transaction_ptr tx,
    uint32_t input_index, size_t last_height, validate_handler handler)
{
    const auto& outpoint = tx->inputs[input_index].previous_output;

    // Search for a spend of this output in the blockchain.
    blockchain_.fetch_spend(outpoint,
        dispatch_.unordered_delegate(&validate_transaction::handle_double_spend,
            shared_from_this(), _1, _2, tx, input_index, last_height,
                handler));
}

// This just determines if the output is spent (or a utxo).
void validate_transaction::handle_double_spend(const code& ec,
    const chain::input_point&, transaction_ptr tx, uint32_t input_index,
    size_t last_height, validate_handler handler)
{
    const auto& outpoint = tx->inputs[input_index].previous_output;

    if (!ec)
    {
        handler(error::double_spend, { input_index }, 0);
        return;
    }

    if (ec != error::not_found)
    {
        handler(ec, { input_index }, 0);
        return;
    }

    if (pool_.is_spent_in_pool(outpoint))
    {
        // TODO: we may want to allow spent-in-pool.
        // This is very costly given the lack of pool indexing,
        // we may want to skip this check until uniform indexing in v4.
        handler(error::double_spend, {}, 0);
        return;
    }

    // TODO: combine query to get height with fetch_transaction.
    const size_t todo_3 = 42;

    // TODO: we just need the output and block height, not full tx,
    // but our tx-based encoding requires we decode the full transaction.
    // We only want the height when it's an output of a coinbase tx.

    // Locate the previous transaction for the input.
    blockchain_.fetch_transaction(outpoint.hash,
        std::bind(&validate_transaction::handle_previous_tx,
            shared_from_this(), _1, _2, todo_3, tx, input_index, last_height,
                handler));
}

void validate_transaction::handle_previous_tx(const code& ec,
    const transaction& previous_tx, size_t previous_tx_height,
    transaction_ptr tx, uint32_t input_index, size_t last_height,
    validate_handler handler)
{
    // This assumes that the mempool is always operating at max block version.
    static constexpr auto script_flags = script_context::all_enabled;

    uint64_t value = 0;
    point::indexes unconfirmed;
    const auto& outpoint = tx->inputs[input_index].previous_output;

    if (ec == error::input_not_found)
    {
        transaction pool_tx;

        // Try locating it as unconfirmed in the memory pool.
        if (!pool_.find(pool_tx, outpoint.hash) ||
            outpoint.index >= pool_tx.outputs.size())
        {
            handler(ec, { input_index }, 0);
            return;
        }

        const auto error_code = check_input(tx, input_index, pool_tx, 0,
            last_height, script_flags, value);

        if (error_code)
        {
            handler(error_code, { input_index }, 0);
            return;
        }

        unconfirmed.push_back(input_index);
    }
    else if (ec)
    {
        handler(ec, { input_index }, 0);
        return;
    }
    else if (outpoint.index >= previous_tx.outputs.size())
    {
        handler(error::input_not_found, { input_index }, 0);
        return;
    }
    else
    {
        const auto error_code = check_input(tx, input_index, previous_tx,
            previous_tx_height, last_height, script_flags, value);

        if (error_code)
        {
            handler(error_code, { input_index }, 0);
            return;
        }
    }

    // Input validation sequence end, triggers handle_complete when full.
    handler(error::success, unconfirmed, value);
}

//-----------------------------------------------------------------------------

// TODO: summarize values in custom stateful synchronizer.
void validate_transaction::handle_join(const code& ec,
    point::indexes unconfirmed, uint64_t total_input_value,
    transaction_ptr tx, validate_handler handler)
{
    if (ec)
    {
        // Forward the failing parameters to the main handler.
        handler(ec, unconfirmed, 0);
        return;
    }

    if (tx->total_output_value() > total_input_value)
    {
        handler(error::spend_exceeds_value, {}, 0);
        return;
    }

    // Who cares?
    // Fuck the police
    // Every tx equal!

    // This is the end of the validate sequence.
    handler(error::success, unconfirmed, total_input_value);
}

// Static utilities used for tx and block validation.
//-----------------------------------------------------------------------------

// pointers (mempool)

// common inexpensive checks
code validate_transaction::check_transaction(const transaction& tx)
{
    if (tx.inputs.empty() || tx.outputs.empty())
        return error::empty_transaction;

    if (tx.serialized_size() > max_transaction_size)
        return error::size_limits;

    if (tx.total_output_value() > max_money())
        return error::output_value_overflow;

    if (tx.is_invalid_coinbase())
        return error::invalid_coinbase_script_size;

    if (tx.is_invalid_non_coinbase())
        return error::previous_output_null;

    if (tx.signature_operations(false) > max_transaction_sigops)
        return error::too_many_sigs;

    return error::success;
}

// TODO: this only needs the specific utxo (output), coinbase testable.
// common expensive checks
code validate_transaction::check_input(const transaction& tx,
    uint32_t input_index, const transaction& previous_tx,
    size_t previous_tx_height, size_t last_height, uint32_t flags,
    uint64_t& output_value)
{
    const auto& input = tx.inputs[input_index];
    const auto& previous_outpoint = input.previous_output;

    if (previous_outpoint.index >= previous_tx.outputs.size())
        return error::input_not_found;

    const auto& previous_output = previous_tx.outputs[previous_outpoint.index];
    output_value = previous_output.value;

    BITCOIN_ASSERT(previous_height <= parent_height);

    if (previous_tx.is_coinbase() &&
        (last_height - previous_tx_height < coinbase_maturity))
        return error::coinbase_maturity;

    if (output_value > max_money())
        return error::output_value_overflow;

    // TODO: Summarize in aggregator.
    // TODO: validate total_output_value() <= value_accumulator.
    ////if (value_accumulator > max_money() - output_value)
    ////    return error::output_value_overflow;
    ////
    ////value_accumulator += output_value;

    return check_script(tx, input_index, previous_output.script, flags);
}

code validate_transaction::check_script(const transaction& tx,
    uint32_t input_index, const script& prevout_script, uint32_t flags)
{
    if (input_index >= tx.inputs.size())
        return error::input_not_found;

#ifdef WITH_CONSENSUS
    using namespace bc::consensus;
    const auto script_data = prevout_script.to_data(false);
    const auto transaction_data = tx.to_data();

    // Convert native flags to libbitcoin-consensus flags.
    uint32_t consensus_flags = verify_flags_none;

    if ((flags & script_context::bip16_enabled) != 0)
        consensus_flags |= verify_flags_p2sh;

    if ((flags & script_context::bip65_enabled) != 0)
        consensus_flags |= verify_flags_checklocktimeverify;

    if ((flags & script_context::bip66_enabled) != 0)
        consensus_flags |= verify_flags_dersig;

    const auto result = verify_script(transaction_data.data(),
        transaction_data.size(), script_data.data(), script_data.size(),
        input_index, consensus_flags);

    const auto valid = (result == verify_result::verify_result_eval_true);
#else
    const auto valid = script::verify(tx.inputs[input_index].script,
        prevout_script, tx, input_index, flags);
#endif

    return valid ? error::success : error::validate_inputs_failed;
}

// references (block)

// common inexpensive checks
code validate_transaction::check_transactions(const block& block)
{
    for (const auto& tx: block.transactions)
    {
        const auto error_code = check_transaction(tx);

        if (error_code)
            return error_code;
    }

    return error::success;
}

code validate_transaction::check_transaction(transaction_ptr tx)
{
    return check_transaction(*tx);
}

code validate_transaction::check_input(transaction_ptr tx,
    uint32_t input_index, const transaction& previous_tx,
    size_t previous_tx_height, size_t last_height, uint32_t flags,
    uint64_t& output_value)
{
    return check_input(*tx, input_index, previous_tx, previous_tx_height,
        last_height, flags, output_value);
}

code validate_transaction::check_script(transaction_ptr tx,
    uint32_t input_index, const script& prevout_script, uint32_t flags)
{
    return check_script(*tx, input_index, prevout_script, flags);
}

} // namespace blockchain
} // namespace libbitcoin
