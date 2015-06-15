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
#include <bitcoin/blockchain/validate.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/checkpoints.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
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

enum validation_options : uint32_t
{
    none,
    p2sh
    // dersig
};

// Determine if BIP16 compliance is required for this block.
static bool is_bip_16_enabled(const block_header_type& header, size_t height)
{
    // Block 170060 contains an invalid BIP 16 transaction before switchover date.
    bool bip16_enabled = header.timestamp >= bip16_switchover_timestamp;
    BITCOIN_ASSERT(!bip16_enabled || height >= bip16_switchover_height);
    return bip16_enabled;
}

// Validate script consensus conformance based on flags provided.
static bool validate_consensus(const script_type& prevout_script,
    const transaction_type& current_tx, size_t input_index,
    uint32_t options)
{
    BITCOIN_ASSERT(input_index < current_tx.inputs.size());
    BITCOIN_ASSERT(input_index <= max_uint32);
    const auto input_index32 = static_cast<uint32_t>(input_index);
    bool bip16_enabled = ((options & validation_options::p2sh) != 0);

#ifdef WITH_CONSENSUS
    using namespace bc::consensus;
    auto previous_output_script = save_script(prevout_script);
    data_chunk current_transaction(satoshi_raw_size(current_tx));
    satoshi_save(current_tx, current_transaction.begin());

    // TODO: expand support beyond BIP16 option.
    const auto flags = (bip16_enabled ? verify_flags_p2sh : verify_flags_none);
    const auto result = verify_script(current_transaction.data(),
        current_transaction.size(), previous_output_script.data(),
        previous_output_script.size(), input_index32, flags);

    BITCOIN_ASSERT(
        (result == verify_result::verify_result_eval_true) ||
        (result == verify_result::verify_result_eval_false));

    return (result == bc::consensus::verify_result::verify_result_eval_true);
#else
    auto previous_output_script = prevout_script;
    auto current_input_script = current_tx.inputs[input_index].script;

    // TODO: expand support beyond BIP16 option.
    return previous_output_script.run(current_input_script, current_tx,
        input_index32, bip16_enabled);
#endif
}

// Validate script consensus conformance, calculating p2sh based on block/height.
static bool validate_consensus(const script_type& prevout_script,
    const transaction_type& current_tx, size_t input_index,
    const block_header_type& header, const size_t height)
{
    uint32_t options = validation_options::none;
    if (is_bip_16_enabled(header, height))
        options = validation_options::p2sh;

    return validate_consensus(prevout_script, current_tx, input_index, options);
}

// Validate script consensus conformance, defaulting to p2sh.
static bool validate_consensus(const script_type& prevout_script,
    const transaction_type& current_tx, size_t input_index)
{
    return validate_consensus(prevout_script, current_tx, input_index, 
        validation_options::p2sh);
}

validate_transaction::validate_transaction(
    blockchain& chain, const chain::transaction& tx,
    const pool_buffer& pool, async_strand& strand)
  : strand_(strand), chain_(chain),
    tx_(tx), tx_hash_(tx.hash()), pool_(pool)
{
}

void validate_transaction::start(validate_handler handle_validate)
{
    handle_validate_ = handle_validate;
    std::error_code ec = basic_checks();
    if (ec)
    {
        handle_validate_(ec, chain::index_list());
        return;
    }

    // Check for duplicates in the blockchain
    chain_.fetch_transaction(tx_hash_,
        strand_.wrap(
            &validate_transaction::handle_duplicate_check,
                shared_from_this(), _1));
}

std::error_code validate_transaction::basic_checks() const
{
    std::error_code ec;
    ec = check_transaction(tx_);
    if (ec)
        return ec;

    if (tx_.is_coinbase())
        return error::coinbase_transaction;

    // Ummm...
    //if ((int64)nLockTime > INT_MAX)

    if (!is_standard())
        return error::is_not_standard;

    // Check for conflicts
    if (fetch(tx_hash_))
        return error::duplicate;
    // Check for blockchain duplicates done next in start() after
    // this function exits.

    return std::error_code();
}

bool validate_transaction::is_standard() const
{
    return true;
}

const chain::transaction* validate_transaction::fetch(
    const hash_digest& tx_hash) const
{
    for (const transaction_entry_info& entry: pool_)
        if (entry.hash == tx_hash)
            return &entry.tx;
    return nullptr;
}

void validate_transaction::handle_duplicate_check(const std::error_code& ec)
{
    if (ec != error::not_found)
    {
        handle_validate_(error::duplicate, chain::index_list());
        return;
    }
    // Check for conflicts with memory txs
    for (size_t input_index = 0; input_index < tx_.inputs.size();
        ++input_index)
    {
        const chain::output_point& previous_output =
            tx_.inputs[input_index].previous_output;
        if (is_spent(previous_output))
        {
            handle_validate_(error::double_spend, chain::index_list());
            return;
        }
    }
    // Check inputs

    // We already know it is not a coinbase tx

    chain_.fetch_last_height(strand_.wrap(
        &validate_transaction::set_last_height, shared_from_this(), _1, _2));
}

bool validate_transaction::is_spent(const chain::output_point& outpoint) const
{
    for (const transaction_entry_info& entry: pool_)
        for (const chain::transaction_input current_input: entry.tx.inputs)
            if (current_input.previous_output == outpoint)
                return true;
    return false;
}

void validate_transaction::set_last_height(
    const std::error_code& ec, size_t last_height)
{
    if (ec)
    {
        handle_validate_(ec, chain::index_list());
        return;
    }
    // Used for checking coinbase maturity
    last_block_height_ = last_height;
    value_in_ = 0;
    BITCOIN_ASSERT(tx_.inputs.size() > 0);
    current_input_ = 0;
    // Begin looping through the inputs, fetching the previous tx
    next_previous_transaction();
}

void validate_transaction::next_previous_transaction()
{
    BITCOIN_ASSERT(current_input_ < tx_.inputs.size());
    // First we fetch the parent block height for a transaction.
    // Needed for checking the coinbase maturity.
    chain_.fetch_transaction_index(
        tx_.inputs[current_input_].previous_output.hash,
        strand_.wrap(
            &validate_transaction::previous_tx_index,
                shared_from_this(), _1, _2));
}

void validate_transaction::previous_tx_index(
    const std::error_code& ec, size_t parent_height)
{
    if (ec)
    {
        search_pool_previous_tx();
    }
    else
    {
        // Now fetch actual transaction body
        BITCOIN_ASSERT(current_input_ < tx_.inputs.size());
        chain_.fetch_transaction(
            tx_.inputs[current_input_].previous_output.hash,
            strand_.wrap(
                &validate_transaction::handle_previous_tx,
                    shared_from_this(), _1, _2, parent_height));
    }
}

void validate_transaction::search_pool_previous_tx()
{
    const hash_digest& previous_tx_hash =
        tx_.inputs[current_input_].previous_output.hash;
    const chain::transaction* previous_tx = fetch(previous_tx_hash);
    if (!previous_tx)
    {
        handle_validate_(error::input_not_found,
            chain::index_list{ current_input_});

        return;
    }
    BITCOIN_ASSERT(!(*previous_tx).is_coinbase());
    // parent_height ignored here as memory pool transactions can
    // never be a coinbase transaction.
    handle_previous_tx(std::error_code(), *previous_tx, 0);
    unconfirmed_.push_back(current_input_);
}

void validate_transaction::handle_previous_tx(const std::error_code& ec,
    const chain::transaction& previous_tx, size_t parent_height)
{
    if (ec)
    {
        handle_validate_(error::input_not_found,
            chain::index_list{ current_input_});
        return;
    }
    // Should check for are inputs standard here...
    if (!connect_input(tx_, current_input_, previous_tx,
        parent_height, last_block_height_, value_in_))
    {
        handle_validate_(error::validate_inputs_failed, {current_input_});
        return;
    }
    // Search for double spends...
    chain_.fetch_spend(tx_.inputs[current_input_].previous_output,
        strand_.wrap(
            &validate_transaction::check_double_spend,
                shared_from_this(), _1));
}

bool validate_transaction::connect_input(
    const chain::transaction& tx, size_t current_input,
    const chain::transaction& previous_tx, size_t parent_height,
    size_t last_block_height, uint64_t& value_in)
{
    const chain::transaction_input& input = tx.inputs[current_input];
    const chain::output_point& previous_outpoint =
        tx.inputs[current_input].previous_output;
    if (previous_outpoint.index >= previous_tx.outputs.size())
        return false;
    const chain::transaction_output& previous_output =
        previous_tx.outputs[previous_outpoint.index];
    uint64_t output_value = previous_output.value;
    if (output_value > max_money())
        return false;
    if (previous_tx.is_coinbase())
    {
        size_t height_difference = last_block_height - parent_height;
        if (height_difference < coinbase_maturity)
            return false;
    }
    if (!validate_consensus(previous_output.script, tx, current_input))
        return false;
    value_in += output_value;
    if (value_in > max_money())
        return false;
    return true;
}

void validate_transaction::check_double_spend(const std::error_code& ec)
{
    if (ec != error::unspent_output)
    {
        BITCOIN_ASSERT(!ec || ec != error::unspent_output);
        handle_validate_(error::double_spend, chain::index_list());
        return;
    }
    // End of connect_input checks
    ++current_input_;
    if (current_input_ < tx_.inputs.size())
    {
        BITCOIN_ASSERT(current_input_ < tx_.inputs.size());
        // Keep looping
        next_previous_transaction();
        return;
    }
    check_fees();
}

bool validate_transaction::tally_fees(const chain::transaction& tx,
    uint64_t value_in, uint64_t& total_fees)
{
    uint64_t value_out = tx.total_output_value();
    if (value_in < value_out)
        return false;
    uint64_t fee = value_in - value_out;
    total_fees += fee;
    if (total_fees > max_money())
        return false;
    return true;
}

void validate_transaction::check_fees()
{
    uint64_t fee = 0;
    if (!tally_fees(tx_, value_in_, fee))
    {
        handle_validate_(error::fees_out_of_range, chain::index_list());
        return;
    }
    // Who cares?
    // Fuck the police
    // Every tx equal!
    handle_validate_(std::error_code(), unconfirmed_);
}

std::error_code validate_transaction::check_transaction(
    const chain::transaction& tx)
{
    if (tx.inputs.empty() || tx.outputs.empty())
        return error::empty_transaction;

    // Maybe not needed since we try to serialise block in CheckBlock()
    //if (exporter_->save(tx, false).size() > max_block_size)
    //    return false;

    // Check for negative or overflow output values
    uint64_t total_output_value = 0;
    for (chain::transaction_output output: tx.outputs)
    {
        if (output.value > max_money())
            return error::output_value_overflow;
        total_output_value += output.value;
        if (total_output_value > max_money())
            return error::output_value_overflow;
    }

    if (tx.is_coinbase())
    {
        const chain::script& coinbase_script = tx.inputs[0].script;
        size_t coinbase_script_size = coinbase_script.satoshi_size();
        if (coinbase_script_size < 2 || coinbase_script_size > 100)
            return error::invalid_coinbase_script_size;
    }
    else
    {
        for (chain::transaction_input input: tx.inputs)
            if (input.previous_output.is_null())
                return error::previous_output_null;
    }

    return std::error_code();
}

validate_block::validate_block(
    size_t height, const chain::block& current_block)
  : height_(height), current_block_(current_block)
{
}

std::error_code validate_block::check_block()
{
    // CheckBlock()
    // These are checks that are independent of context
    // that can be validated before saving an orphan block
    // ...

    // Size limits
    if (current_block_.transactions.empty() ||
        current_block_.transactions.size() > max_block_size ||
        current_block_.satoshi_size() > max_block_size)
    {
        return error::size_limits;
    }

    const chain::block_header& blk_header = current_block_.header;
    const hash_digest current_block_hash = blk_header.hash();
    if (!check_proof_of_work(current_block_hash, blk_header.bits))
        return error::proof_of_work;

    const ptime block_time = from_time_t(blk_header.timestamp);
    const ptime two_hour_future =
        second_clock::universal_time() + hours(2);
    if (block_time > two_hour_future)
        return error::futuristic_timestamp;

    if (!current_block_.transactions[0].is_coinbase())
        return error::first_not_coinbase;
    for (size_t i = 1; i < current_block_.transactions.size(); ++i)
    {
        const chain::transaction& tx = current_block_.transactions[i];
        if (tx.is_coinbase())
            return error::extra_coinbases;
    }

    std::set<hash_digest> unique_txs;
    for (chain::transaction tx: current_block_.transactions)
    {
        std::error_code ec = validate_transaction::check_transaction(tx);
        if (ec)
            return ec;
        unique_txs.insert(tx.hash());
    }
    if (unique_txs.size() != current_block_.transactions.size())
        return error::duplicate;

    // Check that it's not full of nonstandard transactions
    if (legacy_sigops_count() > max_block_script_sig_operations)
        return error::too_many_sigs;

    if (blk_header.merkle != chain::block::generate_merkle_root(
        current_block_.transactions))
    {
        return error::merkle_mismatch;
    }

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
    if (our_value > target)
        return false;

    return true;
}

inline bool within_op_n(chain::opcode code)
{
    uint8_t raw_code = static_cast<uint8_t>(code);
    return static_cast<uint8_t>(chain::opcode::op_1) <= raw_code &&
        raw_code <= static_cast<uint8_t>(chain::opcode::op_16);
}

inline uint8_t decode_op_n(chain::opcode code)
{
    uint8_t raw_code = static_cast<uint8_t>(code);
    BITCOIN_ASSERT(within_op_n(code));
    // Add 1 because we minus opcode::op_1, not the value before.
    return raw_code - static_cast<uint8_t>(chain::opcode::op_1) + 1;
}

inline size_t count_script_sigops(
    const chain::operation_stack& operations, bool accurate)
{
    size_t total_sigs = 0;
    chain::opcode last_opcode = chain::opcode::bad_operation;

    for (const chain::operation& op: operations)
    {
        if (op.code == chain::opcode::checksig ||
            op.code == chain::opcode::checksigverify)
        {
            total_sigs++;
        }
        else if (op.code == chain::opcode::checkmultisig ||
            op.code == chain::opcode::checkmultisigverify)
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
size_t tx_legacy_sigops_count(const chain::transaction& tx)
{
    size_t total_sigs = 0;

    for (chain::transaction_input input: tx.inputs)
    {
        const chain::operation_stack& operations = input.script.operations();
        total_sigs += count_script_sigops(operations, false);
    }

    for (chain::transaction_output output: tx.outputs)
    {
        const chain::operation_stack& operations = output.script.operations();
        total_sigs += count_script_sigops(operations, false);
    }

    return total_sigs;
}

size_t validate_block::legacy_sigops_count()
{
    size_t total_sigs = 0;

    for (chain::transaction tx: current_block_.transactions)
        total_sigs += tx_legacy_sigops_count(tx);

    return total_sigs;
}

std::error_code validate_block::accept_block()
{
    const chain::block_header& blk_header = current_block_.header;

    if (blk_header.bits != work_required())
        return error::incorrect_proof_of_work;

    if (blk_header.timestamp <= median_time_past())
        return error::timestamp_too_early;

    // Txs should be final when included in a block
    for (const chain::transaction& tx: current_block_.transactions)
        if (!tx.is_final(height_, blk_header.timestamp))
            return error::non_final_transaction;

    // See checkpoints.hpp header.
    const hash_digest block_hash = blk_header.hash();

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

// TODO: move to utils.
template<typename Value>
inline Value range_constraint(Value value, Value minimum, Value maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

uint32_t validate_block::work_required()
{
#ifdef ENABLE_TESTNET
    const auto last_non_special_bits = [this]
    {
        // Return the last non-special block
        block_header_type previous_block;
        size_t previous_height = height_;

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
    const auto raw_coinbase = coinbase_script.to_data(false);

    // Try to recreate the expected bytes.
    script_number expect_number(height_);
    chain::script expect_coinbase;
    expect_coinbase.operations.push_back({
        opcode::special, expect_number.data()
    });

    // Save the expected coinbase script.
    const data_chunk expect = expect_coinbase.to_data(false);

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
    for (size_t tx_index = 0; tx_index < current_block_.transactions.size();
            ++tx_index)
    {
        uint64_t value_in = 0;
        const auto& tx = current_block_.transactions[tx_index];
        total_sigops += tx_legacy_sigops_count(tx);
        if (total_sigops > max_block_script_sig_operations)
            return error::too_many_sigs;

        // Count sigops for tx 0, but we don't perform
        // the other checks on coinbase tx.
        if (tx.is_coinbase())
            continue;

        if (!validate_inputs(tx, tx_index, value_in, total_sigops))
            return error::validate_inputs_failed;

        if (!validate_transaction::tally_fees(tx, value_in, fees))
            return error::fees_out_of_range;
    }
    uint64_t coinbase_value =
        current_block_.transactions[0].total_output_value();
    if (coinbase_value  > block_value(height_) + fees)
        return error::coinbase_too_large;

    return std::error_code();
}

bool validate_block::not_duplicate_or_spent(const chain::transaction& tx)
{
    const auto& tx_hash = tx.hash();

    // Is there a matching previous tx?
    if (!transaction_exists(tx_hash))
        return true;

    // For a duplicate tx to exist, all its outputs must have been spent.
    for (uint32_t output = 0; output < tx.outputs.size(); ++output)
        if (!is_output_spent({tx_hash, output}))
            return false;

    return true;
}

bool validate_block::validate_inputs(const chain::transaction& tx,
    size_t index_in_parent, uint64_t& value_in, size_t& total_sigops)
{
    BITCOIN_ASSERT(!tx.is_coinbase());
    for (size_t input_index = 0; input_index < tx.inputs.size(); ++input_index)
    {
        if (!connect_input(index_in_parent, tx, input_index,
                value_in, total_sigops))
        {
            log_warning(LOG_VALIDATE) << "Validate input "
                << encode_hash(tx.hash()) << ":"
                << input_index << " failed";
            return false;
        }
    }
    return true;
}

size_t script_hash_signature_operations_count(
    const chain::script& output_script, const chain::script& input_script)
{
    if (output_script.type() != chain::payment_type::script_hash)
        return 0;

    if (input_script.operations().empty())
        return 0;

    const auto& last_data = input_script.operations.back().data;
    chain::script eval_script;
    eval_script.from_data(last_data, false, false);

    return count_script_sigops(eval_script.operations(), true);
}

bool validate_block::connect_input(size_t index_in_parent,
    const chain::transaction& current_tx, size_t input_index,
    uint64_t& value_in, size_t& total_sigops)
{
    // Lookup previous output
    BITCOIN_ASSERT(input_index < current_tx.inputs.size());

    const auto& input = current_tx.inputs[input_index];
    const auto& previous_output = input.previous_output;
    chain::transaction previous_tx;
    size_t previous_height;

    if (!fetch_transaction(previous_tx, previous_height, previous_output.hash))
    {
        log_warning(LOG_VALIDATE) << "Unable to fetch input transaction";
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
        log_warning(LOG_VALIDATE) << "Parsing eval script failed.";
        return false;
    }

    if (total_sigops > max_block_script_sig_operations)
    {
        log_warning(LOG_VALIDATE) << "Total sigops exceeds block maximum";
        return false;
    }

    // Get output amount
    uint64_t output_value = previous_tx_out.value;
    if (output_value > max_money())
    {
        log_warning(LOG_VALIDATE) << "Total sigops exceeds block maximum";
        return false;
    }

    // Check coinbase maturity has been reached
    if (previous_tx.is_coinbase())
    {
        BITCOIN_ASSERT(previous_height <= height_);
        uint32_t height_difference = height_ - previous_height;
        if (height_difference < coinbase_maturity)
        {
            log_warning(LOG_VALIDATE) << "Spends immature coinbase";
            return false;
        }
    }

    if (!validate_consensus(previous_tx_out.script, current_tx, input_index,
        current_block_.header, height_))
    {
        log_warning(LOG_VALIDATE) << "Input script consensus validation failed";
        return false;
    }

    // Search for double spends
    if (is_output_spent(previous_output, index_in_parent, input_index))
    {
        log_warning(LOG_VALIDATE) << "Double spend detected";
        return false;
    }

    // Increase value_in by this output's value
    value_in += output_value;
    if (value_in > max_money())
    {
        log_warning(LOG_VALIDATE) << "Total input money over 21 million";
        return false;
    }
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
