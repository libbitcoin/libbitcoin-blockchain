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
#include <bitcoin/blockchain/validation/validate_block.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "validate_block"

static constexpr size_t ms_per_second = 1000;

validate_block::validate_block(threadpool& pool, bool testnet,
    bool libconsensus, const checkpoints& checkpoints,
    const simple_chain& chain)
  : chain_state_(testnet, checkpoints),
    history_(chain_state_.sample_size),
    stopped_(false),
    dispatch_(pool, NAME "_dispatch"),
    chain_(chain),
    use_libconsensus_(libconsensus)
{
}

// Stop sequence (thread safe).
//-----------------------------------------------------------------------------

void validate_block::stop()
{
    stopped_.store(true);
}

bool validate_block::stopped() const
{
    return stopped_.load();
}

// Reset (not thread safe).
//-----------------------------------------------------------------------------

// Must call this to update chain state before calling accept or connect.
// There is no need to call a second time for connect after accept.
code validate_block::reset(size_t height)
{
    ///////////////////////////////////////////////////////////////////////////
    // TODO: populate chain state.
    ///////////////////////////////////////////////////////////////////////////
    //// get_block_versions(height, chain_state_.sample_size)
    //// median_time_past(time, height);
    //// work_required(work, height);
    //// work_required_testnet(work, height, candidate_block.timestamp);

    // This has a side effect on subsequent calls!
    chain_state_.set_context(height, history_);
    return error::success;
}

// Validation sequence (thread safe).
//-----------------------------------------------------------------------------

// These checks are context free (no reset).
code validate_block::check(block_const_ptr block) const
{
    return block->check();
}

// These checks require height or other chain context (reset).
code validate_block::accept(block_const_ptr block) const
{
    return block->accept(chain_state_);
}

// These checks require output traversal and validation (reset).
// We do not use block/tx.connect here because we want to fan out by input.
void validate_block::connect(block_const_ptr block,
    result_handler handler) const
{
    if (!chain_state_.use_full_validation())
    {
        log::info(LOG_BLOCKCHAIN)
            << "Block [" << chain_state_.next_height() << "] accepted ("
            << block->transactions.size() << ") txs and ("
            << block->total_inputs() << ") inputs under checkpoint.";
        handler(error::success);
        return;
    }

    const result_handler complete_handler =
        std::bind(&validate_block::handle_connect,
            this, _1, block, asio::steady_clock::now(), handler);

    const result_handler join_handler =
        synchronize(complete_handler, block->total_inputs(), NAME "_sync");

    // Skip the first transaction in order to avoid the coinbase.
    for (const auto& tx: block->transactions)
        for (uint32_t index = 0; index < tx.inputs.size(); ++index)
            dispatch_.concurrent(&validate_block::connect_input,
                this, std::ref(tx), index, join_handler);
}

void validate_block::connect_input(const transaction& tx, uint32_t input_index,
    result_handler handler) const
{
    handler(validate_input(tx, input_index));
}

code validate_block::validate_input(const transaction& tx,
    uint32_t input_index) const
{
    return error::success;

    if (stopped())
        return error::service_stopped;

    if (tx.is_coinbase())
        return error::success;

    if (input_index >= tx.inputs.size())
        return error::input_not_found;

    if (!tx.inputs[input_index].previous_output.is_cached())
        return error::not_found;

    return verify_script(tx, input_index, chain_state_.enabled_forks(),
        use_libconsensus_);
}

void validate_block::handle_connect(const code& ec, block_const_ptr block,
    asio::time_point start_time, result_handler handler) const
{
    const auto delta = asio::steady_clock::now() - start_time;
    const auto elapsed = std::chrono::duration_cast<asio::milliseconds>(delta);
    const auto ms_per_block = static_cast<float>(elapsed.count());
    const auto ms_per_input = ms_per_block / block->total_inputs();
    const auto seconds_per_block = ms_per_block / ms_per_second;

    log::info(LOG_BLOCKCHAIN)
        << "Block [" << chain_state_.next_height() << "] "
        << (ec ? "aborted" : "accepted") << " ("
        << block->transactions.size() << ") txs in ("
        << seconds_per_block << ") secs or ("
        << ms_per_input << ") ms/input";

    handler(ec);
}

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

static code convert_result(consensus::verify_result_type result)
{
    using namespace bc::consensus;
    switch (result)
    {
        // Logical true result.
    case verify_result_type::verify_result_eval_true:
        return error::success;

        // Logical false result.
    case verify_result_type::verify_result_eval_false:
        return error::validate_inputs_failed;

        // Max size errors.
    case verify_result_type::verify_result_script_size:
    case verify_result_type::verify_result_push_size:
    case verify_result_type::verify_result_op_count:
    case verify_result_type::verify_result_stack_size:
    case verify_result_type::verify_result_sig_count:
    case verify_result_type::verify_result_pubkey_count:
        return error::size_limits;

        // Failed verify operations.
    case verify_result_type::verify_result_verify:
    case verify_result_type::verify_result_equalverify:
    case verify_result_type::verify_result_checkmultisigverify:
    case verify_result_type::verify_result_checksigverify:
    case verify_result_type::verify_result_numequalverify:
        return error::validate_inputs_failed;

        // Logical/Format/Canonical errors.
    case verify_result_type::verify_result_bad_opcode:
    case verify_result_type::verify_result_disabled_opcode:
    case verify_result_type::verify_result_invalid_stack_operation:
    case verify_result_type::verify_result_invalid_altstack_operation:
    case verify_result_type::verify_result_unbalanced_conditional:
        return error::validate_inputs_failed;

        // BIP62 errors (should not see these unless requsted).
    case verify_result_type::verify_result_sig_hashtype:
    case verify_result_type::verify_result_sig_der:
    case verify_result_type::verify_result_minimaldata:
    case verify_result_type::verify_result_sig_pushonly:
    case verify_result_type::verify_result_sig_high_s:
    case verify_result_type::verify_result_sig_nulldummy:
    case verify_result_type::verify_result_pubkeytype:
    case verify_result_type::verify_result_cleanstack:
        return error::validate_inputs_failed;

        // Softfork safeness
    case verify_result_type::verify_result_discourage_upgradable_nops:
        return error::validate_inputs_failed;

        // Other
    case verify_result_type::verify_result_op_return:
    case verify_result_type::verify_result_unknown_error:
        return error::validate_inputs_failed;

        // augmention codes for tx deserialization
    case verify_result_type::verify_result_tx_invalid:
    case verify_result_type::verify_result_tx_size_invalid:
    case verify_result_type::verify_result_tx_input_invalid:
        return error::validate_inputs_failed;

        // BIP65 errors
    case verify_result_type::verify_result_negative_locktime:
    case verify_result_type::verify_result_unsatisfied_locktime:
        return error::validate_inputs_failed;

    default:
        return error::validate_inputs_failed;
    }
}

code validate_block::verify_script(const transaction& tx, uint32_t input_index,
    uint32_t flags, bool use_libconsensus)
{
    if (!use_libconsensus)
        return script::verify(tx, input_index, flags);

    BITCOIN_ASSERT(input_index < tx.inputs.size());
    const auto& prevout = tx.inputs[input_index].previous_output;
    const auto transaction_data = tx.to_data();
    const auto script_data = prevout.cache.script.to_data(false);
    return convert_result(consensus::verify_script(transaction_data.data(),
        transaction_data.size(), script_data.data(), script_data.size(),
        input_index, convert_flags(flags)));
}

#else

code validate_block::verify_script(const transaction& tx,
    uint32_t input_index, uint32_t flags, bool)
{
    return script::verify(tx, input_index, flags);
}

#endif

} // namespace blockchain
} // namespace libbitcoin

////bool validate_block::contains_duplicates(block_const_ptr block) const
////{
////    size_t height;
////    transaction other;
////    auto duplicate = false;
////    const auto& txs = block.transactions;
////
////    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
////    for (auto tx = txs.begin(); !duplicate && tx != txs.end(); ++tx)
////    {
////        duplicate = fetch_transaction(other, height, tx->hash());
////    }
////    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
////
////    return duplicate;
////}
////
// Lookup transaction and block height of the previous output.
////if (!fetch_transaction(previous_tx, previous_tx_height, previous_hash))
////    return error::input_not_found;