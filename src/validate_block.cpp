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
#include <bitcoin/blockchain/validate_block.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <boost/date_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoints.hpp>
#include <bitcoin/blockchain/validate_transaction.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace chain {

using boost::posix_time::ptime;
using boost::posix_time::from_time_t;
using boost::posix_time::second_clock;
using boost::posix_time::hours;

constexpr uint32_t max_block_size = 1000000;
constexpr uint32_t max_block_script_sig_operations = max_block_size / 50;

// Every two weeks we readjust target
constexpr uint64_t target_timespan = 14 * 24 * 60 * 60;

// Aim for blocks every 10 mins
constexpr uint64_t target_spacing = 10 * 60;

// Two weeks worth of blocks = readjust interval = 2016
constexpr uint64_t readjustment_interval = target_timespan / target_spacing;

// TODO: move to misc utils.
template<typename Value>
inline Value range_constraint(Value value, Value minimum, Value maximum)
{
    if (value < minimum)
        return minimum;

    if (value > maximum)
        return maximum;

    return value;
}

validate_block::validate_block(size_t height, const block_type& current_block)
  : height_(height), current_block_(current_block)
{
}

std::error_code validate_block::check_block()
{
    // These are checks that are independent of context
    // that can be validated before saving an orphan block

    if (current_block_.transactions.empty() ||
        current_block_.transactions.size() > max_block_size ||
        satoshi_raw_size(current_block_) > max_block_size)
    {
        return error::size_limits;
    }

    const auto& blk_header = current_block_.header;
    const auto current_block_hash = hash_block_header(blk_header);
    if (!check_proof_of_work(current_block_hash, blk_header.bits))
        return error::proof_of_work;

    const auto block_time = from_time_t(blk_header.timestamp);
    const auto two_hour_future = second_clock::universal_time() + hours(2);
    if (block_time > two_hour_future)
        return error::futuristic_timestamp;

    if (!is_coinbase(current_block_.transactions[0]))
        return error::first_not_coinbase;

    for (size_t index = 1; index < current_block_.transactions.size(); ++index)
        if (is_coinbase(current_block_.transactions[index]))
            return error::extra_coinbases;

    std::set<hash_digest> unique_txs;
    for (const auto& tx: current_block_.transactions)
    {
        const auto ec = validate_transaction::check_transaction(tx);
        if (ec)
            return ec;

        unique_txs.insert(hash_transaction(tx));
    }

    if (unique_txs.size() != current_block_.transactions.size())
        return error::duplicate;

    // Check that it's not full of nonstandard transactions
    if (legacy_sigops_count() > max_block_script_sig_operations)
        return error::too_many_sigs;

    if (blk_header.merkle != generate_merkle_root(current_block_.transactions))
        return error::merkle_mismatch;

    return std::error_code();
}

bool validate_block::check_proof_of_work(hash_digest block_hash, uint32_t bits)
{
    hash_number target;
    if (!target.set_compact(bits))
        return false;

    if (target <= 0 || target > max_target())
        return false;

    hash_number our_value;
    our_value.set_hash(block_hash);
    return (our_value <= target);
}

inline bool within_op_n(opcode code)
{
    const auto raw_code = static_cast<uint8_t>(code);
    constexpr auto op_1 = static_cast<uint8_t>(opcode::op_1);
    constexpr auto op_16 = static_cast<uint8_t>(opcode::op_16);
    return op_1 <= raw_code && raw_code <= op_16;
}
inline uint8_t decode_op_n(opcode code)
{
    const auto raw_code = static_cast<uint8_t>(code);
    BITCOIN_ASSERT(within_op_n(code));

    // Add 1 because we minus opcode::op_1, not the value before.
    constexpr auto op_1 = static_cast<uint8_t>(opcode::op_1);
    return raw_code - op_1 + 1;
}

inline size_t count_script_sigops(
    const operation_stack& operations, bool accurate)
{
    size_t total_sigs = 0;
    opcode last_opcode = opcode::bad_operation;
    for (const auto& op: operations)
    {
        if (op.code == opcode::checksig ||
            op.code == opcode::checksigverify)
        {
            total_sigs++;
        }
        else if (op.code == opcode::checkmultisig ||
            op.code == opcode::checkmultisigverify)
        {
            if (accurate && within_op_n(last_opcode))
                total_sigs += decode_op_n(last_opcode);
            else
                total_sigs += 20;
        }

        last_opcode = op.code;
    }

    return total_sigs;
}
size_t tx_legacy_sigops_count(const transaction_type& tx)
{
    size_t total_sigs = 0;
    for (const auto& input: tx.inputs)
    {
        const auto& operations = input.script.operations();
        total_sigs += count_script_sigops(operations, false);
    }

    for (const auto& output : tx.outputs)
    {
        const auto& operations = output.script.operations();
        total_sigs += count_script_sigops(operations, false);
    }

    return total_sigs;
}
size_t validate_block::legacy_sigops_count()
{
    size_t total_sigs = 0;
    for (const auto& tx: current_block_.transactions)
        total_sigs += tx_legacy_sigops_count(tx);

    return total_sigs;
}

std::error_code validate_block::accept_block()
{
    const auto& blk_header = current_block_.header;
    if (blk_header.bits != work_required())
        return error::incorrect_proof_of_work;

    if (blk_header.timestamp <= median_time_past())
        return error::timestamp_too_early;

    // Txs should be final when included in a block
    for (const auto& tx: current_block_.transactions)
        if (!is_final(tx, height_, blk_header.timestamp))
            return error::non_final_transaction;

    // See checkpoints.hpp header.
    const auto block_hash = hash_block_header(blk_header);
    if (!passes_checkpoints(height_, block_hash))
        return error::checkpoints_failed;

    // Reject version=1 blocks after switchover point.
    if (height_ > 237370 && blk_header.version < 2)
        return error::old_version_block;

    // Enforce version=2 rule that coinbase starts with serialized height.
    if (blk_header.version >= 2 && !coinbase_height_match())
        return error::coinbase_height_mismatch;

    return std::error_code();
}

uint32_t validate_block::work_required()
{
#ifdef ENABLE_TESTNET
    const auto last_non_special_bits = [this]
    {
        // Return the last non-special block
        block_header_type previous_block;
        auto previous_height = height_;

        // Loop backwards until we find a difficulty change point,
        // or we find a block which does not have max_bits (is not special).
        while (true)
        {
            --previous_height;
            if (previous_height % readjustment_interval == 0)
               break;

            previous_block = fetch_block(previous_height);
            if (previous_block.bits != max_work_bits)
               break;
        }

        return previous_block.bits;
    };
#endif

    if (height_ == 0)
        return max_work_bits;

    if (height_ % readjustment_interval != 0)
    {
#ifdef ENABLE_TESTNET
        uint32_t max_time_gap =
            fetch_block(height_ - 1).timestamp + 2 * target_spacing;
        if (current_block_.header.timestamp > max_time_gap)
            return max_work_bits;

        return last_non_special_bits();
#else
        return previous_block_bits();
#endif
    }

    // This is the total time it took for the last 2016 blocks.
    const auto actual = actual_timespan(readjustment_interval);

    // Now constrain the time between an upper and lower bound.
    const auto constrained = range_constraint(
        actual, target_timespan / 4, target_timespan * 4);

    hash_number retarget;
    retarget.set_compact(previous_block_bits());
    retarget *= constrained;
    retarget /= target_timespan;
    if (retarget > max_target())
        retarget = max_target();

    return retarget.compact();
}

bool validate_block::coinbase_height_match()
{
    // There are old blocks with version incorrectly set to 2. Ignore them.
    if (height_ < 237370)
        return true;

    // Checks whether the block height is in the coinbase tx input script.
    // Version 2 blocks and onwards.
    BITCOIN_ASSERT(current_block_.header.version >= 2);
    BITCOIN_ASSERT(current_block_.transactions.size() > 0);
    BITCOIN_ASSERT(current_block_.transactions[0].inputs.size() > 0);

    // First get the serialized coinbase input script as a series of bytes.
    const auto& coinbase_tx = current_block_.transactions[0];
    const auto& coinbase_script = coinbase_tx.inputs[0].script;
    const auto raw_coinbase = save_script(coinbase_script);

    // Try to recreate the expected bytes.
    script_type expect_coinbase;
    script_number expect_number(height_);
    expect_coinbase.push_operation({opcode::special, expect_number.data()});

    // Save the expected coinbase script.
    const data_chunk expect = save_script(expect_coinbase);

    // Perform comparison of the first bytes with raw_coinbase.
    BITCOIN_ASSERT(expect.size() <= raw_coinbase.size());
    return std::equal(expect.begin(), expect.end(), raw_coinbase.begin());
}

std::error_code validate_block::connect_block()
{
    // BIP 30 security fix
    if (height_ != 91842 && height_ != 91880)
        for (const auto& current_tx: current_block_.transactions)
            if (!not_duplicate_or_spent(current_tx))
                return error::duplicate_or_spent;

    uint64_t fees = 0;
    size_t total_sigops = 0;
    const auto count = current_block_.transactions.size();
    for (size_t tx_index = 0; tx_index < count; ++tx_index)
    {
        uint64_t value_in = 0;
        const auto& tx = current_block_.transactions[tx_index];
        total_sigops += tx_legacy_sigops_count(tx);
        if (total_sigops > max_block_script_sig_operations)
            return error::too_many_sigs;

        // Count sigops for tx 0, but we don't perform
        // the other checks on coinbase tx.
        if (is_coinbase(tx))
            continue;

        if (!validate_inputs(tx, tx_index, value_in, total_sigops))
            return error::validate_inputs_failed;

        if (!validate_transaction::tally_fees(tx, value_in, fees))
            return error::fees_out_of_range;
    }

    const auto& coinbase = current_block_.transactions[0];
    const auto coinbase_value = total_output_value(coinbase);
    if (coinbase_value > block_value(height_) + fees)
        return error::coinbase_too_large;

    return std::error_code();
}

bool validate_block::not_duplicate_or_spent(const transaction_type& tx)
{
    const auto& tx_hash = hash_transaction(tx);

    // Is there a matching previous tx?
    if (!transaction_exists(tx_hash))
        return true;

    // For a duplicate tx to exist, all its outputs must have been spent.
    for (uint32_t output = 0; output < tx.outputs.size(); ++output)
        if (!is_output_spent({tx_hash, output}))
            return false;

    return true;
}

bool validate_block::validate_inputs(const transaction_type& tx,
    size_t index_in_parent, uint64_t& value_in, size_t& total_sigops)
{
    BITCOIN_ASSERT(!is_coinbase(tx));
    for (size_t input_index = 0; input_index < tx.inputs.size(); ++input_index)
    {
        if (!connect_input(index_in_parent, tx, input_index, value_in,
            total_sigops))
        {
            log_warning(LOG_VALIDATE) << "Invalid input ["
                << encode_hash(hash_transaction(tx)) << ":"
                << input_index << "]";
            return false;
        }
    }
    return true;
}

size_t script_hash_signature_operations_count(
    const script_type& output_script, const script_type& input_script)
{
    if (output_script.type() != payment_type::script_hash)
        return 0;

    if (input_script.operations().empty())
        return 0;

    const auto& last_data = input_script.operations().back().data;
    const auto eval_script = parse_script(last_data);
    return count_script_sigops(eval_script.operations(), true);
}

bool validate_block::connect_input(size_t index_in_parent,
    const transaction_type& current_tx, size_t input_index, uint64_t& value_in,
    size_t& total_sigops)
{
    BITCOIN_ASSERT(input_index < current_tx.inputs.size());

    // Lookup previous output
    size_t previous_height;
    transaction_type previous_tx;
    const auto& input = current_tx.inputs[input_index];
    const auto& previous_output = input.previous_output;
    if (!fetch_transaction(previous_tx, previous_height, previous_output.hash))
    {
        log_warning(LOG_VALIDATE) << "Failure fetching input transaction.";
        return false;
    }

    const auto& previous_tx_out = previous_tx.outputs[previous_output.index];

    // Signature operations count if script_hash payment type.
    try
    {
        total_sigops += script_hash_signature_operations_count(
            previous_tx_out.script, input.script);
    }
    catch (end_of_stream)
    {
        log_warning(LOG_VALIDATE) << "Invalid eval script.";
        return false;
    }

    if (total_sigops > max_block_script_sig_operations)
    {
        log_warning(LOG_VALIDATE) << "Total sigops exceeds block maximum.";
        return false;
    }

    // Get output amount
    const auto output_value = previous_tx_out.value;
    if (output_value > max_money())
    {
        log_warning(LOG_VALIDATE) << "Output money exceeds 21 million.";
        return false;
    }

    // Check coinbase maturity has been reached
    if (is_coinbase(previous_tx))
    {
        BITCOIN_ASSERT(previous_height <= height_);
        const auto height_difference = height_ - previous_height;
        if (height_difference < coinbase_maturity)
        {
            log_warning(LOG_VALIDATE) << "Immature coinbase spend attempt.";
            return false;
        }
    }

    if (!validate_transaction::validate_consensus(previous_tx_out.script,
        current_tx, input_index, current_block_.header, height_))
    {
        log_warning(LOG_VALIDATE) << "Input script invalid consensus.";
        return false;
    }

    // Search for double spends
    if (is_output_spent(previous_output, index_in_parent, input_index))
    {
        log_warning(LOG_VALIDATE) << "Double spend attempt.";
        return false;
    }

    // Increase value_in by this output's value
    value_in += output_value;
    if (value_in > max_money())
    {
        log_warning(LOG_VALIDATE) << "Input money exceeds 21 million.";
        return false;
    }

    return true;
}

} // namespace chain
} // namespace libbitcoin
