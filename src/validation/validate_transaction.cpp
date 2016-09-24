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
#include <bitcoin/blockchain/validation/validate_transaction.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace std::placeholders;

#define NAME "validate_transaction"

#ifdef WITH_CONSENSUS
static uint32_t convert_flags(uint32_t native_flags)
{
    using namespace bc::consensus;
    uint32_t consensus_flags = verify_flags_none;

    if (script::is_enabled(native_flags, rule_fork::bip16_rule))
        consensus_flags |= verify_flags_p2sh;

    if (script::is_enabled(native_flags, rule_fork::bip65_rule))
        consensus_flags |= verify_flags_checklocktimeverify;

    if (script::is_enabled(native_flags, rule_fork::bip66_rule))
        consensus_flags |= verify_flags_dersig;

    return consensus_flags;
}
#endif

validate_transaction::validate_transaction(full_chain& chain,
    const transaction_pool& pool, dispatcher& dispatch)
  : blockchain_(chain),
    pool_(pool),
    dispatch_(dispatch),
    use_libconsensus_(false)
{
}

void validate_transaction::validate(transaction_const_ptr tx,
    validate_handler handler)
{
    auto ec = tx->check();

    if (ec)
    {
        handler(ec, {});
        return;
    }

    // BIP30 is presumed here to be always active WRT mempool transactions.
    // Check for a duplicate transaction identifier (hash) existence.
    blockchain_.fetch_transaction_position(tx->hash(),
        std::bind(&validate_transaction::handle_duplicate,
            shared_from_this(), _1, _2, _3, tx, handler));
}

void validate_transaction::handle_duplicate(const code& ec, uint64_t, uint64_t,
    transaction_const_ptr tx, validate_handler handler)
{
    ////const auto ec = tx->connect(state);

    ////if (ec)
    ////{
    ////    handler(ec, {});
    ////    return;
    ////}

    ////if (pool_.is_in_pool(tx->hash()))
    ////    return;

    // Get chain height for determining coinbase maturity.
    blockchain_.fetch_last_height(
        dispatch_.unordered_delegate(&validate_transaction::handle_last_height,
            shared_from_this(), _1, _2, tx, handler));
}

void validate_transaction::handle_last_height(const code& ec,
    size_t last_height, transaction_const_ptr tx, validate_handler handler)
{
    const validate_handler rejoin =
        std::bind(&validate_transaction::handle_join,
            shared_from_this(), _1, _2, tx, handler);

    const validate_handler complete =
        synchronize(rejoin, tx->inputs.size(), NAME);

    // Asynchronously loop all inputs.
    for (uint32_t index = 0; index < tx->inputs.size(); ++index)
        dispatch_.concurrent(&validate_transaction::validate_input,
            shared_from_this(), tx, index, last_height, complete);
}

void validate_transaction::validate_input(transaction_const_ptr tx,
    uint32_t input_index, size_t last_height, validate_handler handler)
{
    const auto& outpoint = tx->inputs[input_index].previous_output;

    // Search for a spend of this output in the blockchain.
    blockchain_.fetch_spend(outpoint,
        dispatch_.unordered_delegate(&validate_transaction::handle_double_spend,
            shared_from_this(), _1, _2, tx, input_index, last_height, handler));
}

// This just determines if the output is spent (or a utxo).
void validate_transaction::handle_double_spend(const code& ec,
    const chain::input_point&, transaction_const_ptr tx, uint32_t input_index,
    size_t last_height, validate_handler handler)
{
    const auto& outpoint = tx->inputs[input_index].previous_output;

    ////if (pool_.is_spent_in_pool(outpoint))
    ////    return;

    ////// Locate the previous transaction for the input.
    ////blockchain_.fetch_transaction(outpoint.hash,
    ////    dispatch_.unordered_delegate(&validate_transaction::handle_previous_tx,
    ////        shared_from_this(), _1, _2, _3, tx, input_index, last_height,
    ////            handler));
}

void validate_transaction::handle_previous_tx(const code& ec,
    const transaction& previous_tx, uint64_t previous_tx_height,
    transaction_const_ptr tx, uint32_t input_index, size_t last_height,
    validate_handler handler)
{
    const auto& outpoint = tx->inputs[input_index].previous_output;

    if (ec == error::input_not_found)
    {
        ////const auto pool_tx = pool_.find(outpoint.hash);

        ////// Try locating it as unconfirmed in the memory pool.
        ////if (!pool_tx || outpoint.index >= pool_tx->outputs.size())
        ////    return;
    }

    const auto previous_height = static_cast<size_t>(previous_tx_height);

    ////const auto error_code = check_input(tx, input_index, previous_tx,
    ////    previous_height, last_height, script_flags, value);

    // Input validation sequence end, triggers handle_complete when full.
    handler(error::success, {});
}

//-----------------------------------------------------------------------------

void validate_transaction::handle_join(const code& ec,
    const indexes& unconfirmed, transaction_const_ptr tx,
    validate_handler handler)
{

    // Who cares?
    // Fuck the police
    // Every tx equal!

    // This is the end of the validate sequence.
    handler(error::success, unconfirmed);
}

// Static utilities used for tx and block validation.
//-----------------------------------------------------------------------------

// pointers (mempool)

// common expensive checks
code validate_transaction::check_input(const transaction& tx,
    uint32_t input_index, const transaction& previous_tx,
    size_t previous_tx_height, size_t last_height, uint32_t flags,
    uint64_t& output_value)
{
    const auto& input = tx.inputs[input_index];
    const auto& previous_outpoint = input.previous_output;
    const auto& previous_output = previous_tx.outputs[previous_outpoint.index];
    return check_script(tx, input_index, previous_output.script, flags);
}

// references (block)

code validate_transaction::check_script(transaction_const_ptr tx,
    uint32_t input_index, const script& prevout_script, uint32_t flags)
{
    return check_script(*tx, input_index, prevout_script, flags);
}

code validate_transaction::check_script(const transaction& tx,
    uint32_t input_index, const script& prevout_script, uint32_t flags)
{
    if (input_index >= tx.inputs.size())
        return error::input_not_found;

#ifdef WITH_CONSENSUS
    // Convert native flags to libbitcoin-consensus flags.
    uint32_t consensus_flags = convert_flags(flags);

    // Serialize objects.
    const auto script_data = prevout_script.to_data(false);
    const auto transaction_data = tx.to_data();

    using namespace bc::consensus;
    const auto result = verify_script(transaction_data.data(),
        transaction_data.size(), script_data.data(), script_data.size(),
        input_index, consensus_flags);

    const auto valid = (result == verify_result::verify_result_eval_true);
#else
    const auto valid = script::verify(tx, input_index, flags);
#endif

    return valid ? error::success : error::validate_inputs_failed;
}

} // namespace blockchain
} // namespace libbitcoin
