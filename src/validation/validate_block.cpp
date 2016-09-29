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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace bc::error;
using namespace std::placeholders;

#define NAME "validate_block"

validate_block::validate_block(threadpool& pool, const simple_chain& chain,
    const settings& settings)
  : stopped_(false),
    chain_(chain),
    use_libconsensus_(settings.use_libconsensus),
    use_testnet_rules_(settings.use_testnet_rules),
    checkpoints_(checkpoint::sort(settings.checkpoints)),
    dispatch_(pool, NAME "_dispatch"),
    chain_state_(use_testnet_rules_, checkpoints_),
    history_(chain_state_.sample_size)
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

// Populate sequence (not thread safe).
//-----------------------------------------------------------------------------

// TODO: provide fail-fast configuration option (any_of vs. for_each).
void validate_block::populate(fork::const_ptr fork, size_t index,
    result_handler handler)
{
    populate_chain_state(fork, index, nullptr);
    populate_block(fork, index, handler);
}

///////////////////////////////////////////////////////////////////////////////
// TODO: make block state a member of block.validation.
///////////////////////////////////////////////////////////////////////////////
// TODO: precalculate block state on start and after each block push,
// storing a reference in blockchain for use by memory pool and copying to
// blocks as they are received into a fork.
///////////////////////////////////////////////////////////////////////////////
void validate_block::populate_chain_state(fork::const_ptr fork , size_t index,
    result_handler)
{
    ///////////////////////////////////////////////////////////////////////////
    // TODO: populate chain state.
    ///////////////////////////////////////////////////////////////////////////
    //// get_block_versions(height, chain_state_.sample_size)
    //// median_time_past(time, height);
    //// work_required(work, height);
    //// work_required_testnet(work, height, candidate_block.timestamp);
    ///////////////////////////////////////////////////////////////////////////

    // This has a side effect on subsequent calls.
    chain_state_.set_context(fork->height_at(index), history_);
}

void validate_block::populate_block(fork::const_ptr fork, size_t index,
    result_handler handler)
{
    const auto fork_height = fork->height();

    // TODO: parallelize by input, change operation_failed to throw.
    const auto out = [&](error_code_t ec, const transaction& tx)
    {
        const auto in = [&](error_code_t ec, const input& input)
        {
            // This is the only failure case, a database integrity fault.
            if (!populate_spent(fork_height, input.previous_output))
                return error::operation_failed; 

            populate_spent(fork, index, input.previous_output);
            populate_prevout(fork_height, input.previous_output);
            populate_prevout(fork, index, input.previous_output);
            return ec;
        };

        populate_transaction(tx);
        populate_transaction(fork, index, tx);

        const auto& ins = tx.inputs;
        return std::accumulate(ins.begin(), ins.end(), ec, in);
    };

    const auto start_time = asio::steady_clock::now();
    const auto block = fork->block_at(index);
    populate_coinbase(block);

    const auto& txs = block->transactions;
    auto ec = std::accumulate(txs.begin() + 1, txs.end(), error::success, out);

    // TODO: move to timer class managed by orphan pool manager.
    //=========================================================================
    static constexpr size_t micro_per_milli = 1000;
    const auto delta = asio::steady_clock::now() - start_time;
    const auto elapsed = std::chrono::duration_cast<asio::microseconds>(delta);
    const auto micro_per_block = static_cast<float>(elapsed.count());
    const auto micro_per_input = micro_per_block / block->total_inputs();
    const auto milli_per_block = micro_per_block / micro_per_milli;

    log::info(LOG_BLOCKCHAIN)
        << "Block [" << chain_state_.next_height() << "] "
        << (code(ec) ? "unpopulated" : "populated") << " (" << txs.size()
        << ") txs in (" << milli_per_block << ") ms or (" << micro_per_input
        << ") μs/input";
    //=========================================================================

    handler(ec);
}

// Returns false only when transaction is duplicate on chain.
void validate_block::populate_transaction(const chain::transaction& tx) const
{
    // BUGBUG: this is overly restrictive, see BIP30.
    tx.validation.duplicate = chain_.get_is_unspent_transaction(tx.hash());
}

// Returns false only when transaction is duplicate on fork.
void validate_block::populate_transaction(fork::const_ptr fork, size_t index,
    const chain::transaction& tx) const
{
    // Distinctness is a static check so this looks only below the index.
    if (!tx.validation.duplicate)
        fork->populate_tx(index, tx);
}

// Returns false only when database operation fails.
void validate_block::populate_spent(fork::const_ptr fork, size_t index,
    const output_point& outpoint) const
{
    if (!outpoint.validation.spent)
        fork->populate_spent(index, outpoint);
}

void validate_block::populate_prevout(fork::const_ptr fork, size_t index,
    const output_point& outpoint) const
{
    if (!outpoint.validation.cache.is_valid())
        fork->populate_prevout(index, outpoint);
}

// Initialize the coinbase input for subsequent validation.
void validate_block::populate_coinbase(block_const_ptr block) const
{
    const auto& txs = block->transactions;

    // This is only a guard against invalid input.
    if (txs.empty() || !txs.front().is_coinbase())
        return;

    // A coinbase tx guarnatees exactly one input.
    const auto& input = txs.front().inputs.front();
    auto& prevout = input.previous_output.validation;

    // A coinbase input cannot be a double spend since it originates coin.
    prevout.spent = false;

    // A coinbase is only valid within a block and input is confirmed if valid.
    prevout.confirmed = true;

    // A coinbase input has no previous output.
    prevout.cache.reset();

    // A coinbase input does not spend an output so is itself always mature.
    prevout.height = output_point::validation::not_specified;
}

// Returns false only when database operation fails.
bool validate_block::populate_spent(size_t fork_height,
    const output_point& outpoint) const
{
    size_t spender_height;
    hash_digest spender_hash;
    auto& prevout = outpoint.validation;

    // Confirmed state matches spend state (unless tx pool validation).
    prevout.confirmed = false;

    // Determine if the prevout is spent by a confirmed input.
    prevout.spent = chain_.get_spender_hash(spender_hash, outpoint);

    // Either the prevout is unspent, spent in fork, the outpoint is invalid.
    if (!prevout.spent)
        return true;

    // It is a store failure it the spender transaction is not found.
    if (!chain_.get_transaction_height(spender_height, spender_hash))
        return false;

    // Unspend the prevout if it is above the fork.
    prevout.spent = spender_height <= fork_height;

    // All block spends are confirmed spends.
    prevout.confirmed = prevout.spent;
    return true;
}

void validate_block::populate_prevout(size_t fork_height,
    const output_point& outpoint) const
{
    size_t height;
    size_t position;
    auto& prevout = outpoint.validation;

    // In case this input is a coinbase or the prevout is spent.
    prevout.cache.reset();

    // The height of the prevout must be set iff the prevout is coinbase.
    prevout.height = output_point::validation::not_specified;

    // The input is a coinbase, so there is no prevout to populate.
    if (outpoint.is_null())
        return;

    ////// The prevout is spent, so don't bother to populate it [optimized].
    ////if (prevout.spent)
    ////    return true;

    // Get the script and value for the prevout.
    if (!chain_.get_output(prevout.cache, height, position, outpoint))
        return;

    // Unfind the prevout if it is above the fork (clear the cache).
    if (height > fork_height)
    {
        prevout.cache.reset();
        return;
    }

    // Set height iff the prevout is coinbase (first tx is coinbase).
    if (position == 0)
        prevout.height = height;
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
        //=====================================================================
        log::info(LOG_BLOCKCHAIN)
            << "Block [" << chain_state_.next_height() << "] accepted ("
            << block->transactions.size() << ") txs and ("
            << block->total_inputs() << ") inputs under checkpoint.";
        //=====================================================================

        handler(error::success);
        return;
    }

    const result_handler complete_handler =
        std::bind(&validate_block::handle_connect,
            this, _1, block, asio::steady_clock::now(), handler);

    const result_handler join_handler =
        synchronize(complete_handler, block->total_inputs(), NAME "_join");

    // We must always continue on a new thread.
    for (const auto& tx: block->transactions)
        for (size_t index = 0; index < tx.inputs.size(); ++index)
            dispatch_.concurrent(&validate_block::connect_input,
                this, std::ref(tx), index, join_handler);
}

void validate_block::connect_input(const transaction& tx, size_t input_index,
    result_handler handler) const
{
    handler(validate_input(tx, input_index));
}

code validate_block::validate_input(const transaction& tx,
    size_t input_index) const
{
    if (stopped())
        return error::service_stopped;

    if (tx.is_coinbase())
        return error::success;

    if (input_index >= tx.inputs.size())
        return error::operation_failed;

    const auto index32 = static_cast<uint32_t>(input_index);
    const auto& prevout = tx.inputs[input_index].previous_output.validation;

    if (!prevout.cache.is_valid())
        return error::not_found;

    return verify_script(tx, index32, chain_state_.enabled_forks(),
        use_libconsensus_);
}

void validate_block::handle_connect(const code& ec, block_const_ptr block,
    asio::time_point start_time, result_handler handler) const
{
    //=========================================================================
    static constexpr size_t micro_per_milli = 1000;
    const auto delta = asio::steady_clock::now() - start_time;
    const auto elapsed = std::chrono::duration_cast<asio::microseconds>(delta);
    const auto micro_per_block = static_cast<float>(elapsed.count());
    const auto micro_per_input = micro_per_block / block->total_inputs();
    const auto milli_per_block = micro_per_block / micro_per_milli;

    log::info(LOG_BLOCKCHAIN)
        << "Block [" << chain_state_.next_height() << "] "
        << (ec ? "invalidated" : "validated") << " ("
        << block->transactions.size() << ") txs in (" << milli_per_block
        << ") ms or (" << micro_per_input << ") μs/input";
    //=========================================================================

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
    const auto& prevout = tx.inputs[input_index].previous_output.validation;
    const auto script_data = prevout.cache.script.to_data(false);
    const auto tx_data = tx.to_data();

    // libconsensus
    return convert_result(consensus::verify_script(tx_data.data(),
        tx_data.size(), script_data.data(), script_data.size(), input_index,
        convert_flags(flags)));
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
