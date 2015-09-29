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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoint.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;

// [April 1 2012]
static constexpr uint32_t bip16_switchover_timestamp = 1333238400;
////static constexpr uint32_t bip16_switchover_height_mainnet = 173805;
////static constexpr uint32_t bip16_switchover_height_testnet = 514;

enum validation_options : uint32_t
{
    none,
    p2sh
    // dersig
};

validate_transaction::validate_transaction(blockchain& chain,
    const chain::transaction& tx, const pool_buffer& pool,
    dispatcher& dispatch)
  : blockchain_(chain),
    tx_(tx),
    pool_(pool),
    dispatch_(dispatch),
    tx_hash_(tx.hash())
{
}

void validate_transaction::start(validate_handler handle_validate)
{
    handle_validate_ = handle_validate;
    const auto ec = basic_checks();
    if (ec)
    {
        handle_validate_(ec, chain::index_list());
        return;
    }

    // Check for duplicates in the blockchain.
    blockchain_.fetch_transaction(tx_hash_,
        dispatch_.unordered_delegate(
            &validate_transaction::handle_duplicate_check,
                shared_from_this(), _1));
}

std::error_code validate_transaction::basic_checks() const
{
    const auto ec = check_transaction(tx_);
    if (ec)
        return ec;

    if (tx_.is_coinbase())
        return error::coinbase_transaction;

    // Ummm...
    //if ((int64)nLockTime > INT_MAX)

    if (!is_standard())
        return error::is_not_standard;

    if (is_tx_in_pool(tx_hash_))
        return error::duplicate;

    // Check for blockchain duplicates in start (after this returns).
    return error::success;
}

bool validate_transaction::is_standard() const
{
    return true;
}

void validate_transaction::handle_duplicate_check(
    const std::error_code& ec)
{
    if (ec != error::not_found)
    {
        handle_validate_(error::duplicate, chain::index_list());
        return;
    }

    if (is_spent_in_pool(tx_))
    {
        handle_validate_(error::double_spend, chain::index_list());
        return;
    }

    // Check inputs, we already know it is not a coinbase tx.
    blockchain_.fetch_last_height(
        dispatch_.unordered_delegate(&validate_transaction::set_last_height,
            shared_from_this(), _1, _2));
}

pool_buffer::const_iterator validate_transaction::find_tx_in_pool(
    const hash_digest& hash) const
{
    const auto found = [&hash](const transaction_entry_info& entry)
    {
        return entry.hash == hash;
    };

    return std::find_if(pool_.begin(), pool_.end(), found);
}

bool validate_transaction::is_tx_in_pool(const hash_digest& hash) const
{
    return find_tx_in_pool(hash) != pool_.end();
}

bool validate_transaction::is_spent_in_pool(const chain::transaction& tx) const
{
    const auto found = [this, &tx](const chain::input& input)
    {
        return is_spent_in_pool(input.previous_output);
    };

    const auto& inputs = tx.inputs;
    const auto spend = std::find_if(inputs.begin(), inputs.end(), found);
    return spend != inputs.end();
}

bool validate_transaction::is_spent_in_pool(
    const chain::output_point& outpoint) const
{
    const auto found = [this, &outpoint](const transaction_entry_info& entry)
    {
        return is_spent_in_tx(outpoint, entry.tx);
    };

    const auto spend = std::find_if(pool_.begin(), pool_.end(), found);
    return spend != pool_.end();
}

bool validate_transaction::is_spent_in_tx(const chain::output_point& outpoint,
    const chain::transaction& tx) const
{
    const auto found = [&outpoint](const chain::input& input)
    {
        return input.previous_output == outpoint;
    };

    const auto& inputs = tx.inputs;
    const auto spend = std::find_if(inputs.begin(), inputs.end(), found);
    return spend != inputs.end();
}

void validate_transaction::set_last_height(const std::error_code& ec,
    size_t last_height)
{
    if (ec)
    {
        handle_validate_(ec, chain::index_list());
        return;
    }

    // Used for checking coinbase maturity
    last_block_height_ = last_height;
    current_input_ = 0;
    value_in_ = 0;

    // Begin looping through the inputs, fetching the previous tx.
    if (!tx_.inputs.empty())
        next_previous_transaction();
}

void validate_transaction::next_previous_transaction()
{
    BITCOIN_ASSERT(current_input_ < tx_.inputs.size());

    // First we fetch the parent block height for a transaction.
    // Needed for checking the coinbase maturity.
    blockchain_.fetch_transaction_index(
        tx_.inputs[current_input_].previous_output.hash,
            dispatch_.unordered_delegate(
                &validate_transaction::previous_tx_index,
                    shared_from_this(), _1, _2));
}

void validate_transaction::previous_tx_index(const std::error_code& ec,
    size_t parent_height)
{
    if (ec)
    {
        search_pool_previous_tx();
        return;
    }

    BITCOIN_ASSERT(current_input_ < tx_.inputs.size());
    const auto& prev_tx_hash = tx_.inputs[current_input_].previous_output.hash;
    
    // Now fetch actual transaction body
    blockchain_.fetch_transaction(prev_tx_hash,
        dispatch_.unordered_delegate(&validate_transaction::handle_previous_tx,
            shared_from_this(), _1, _2, parent_height));
}

void validate_transaction::search_pool_previous_tx()
{
    const auto& prev_tx_hash = tx_.inputs[current_input_].previous_output.hash;
    const auto previous_tx_info = find_tx_in_pool(prev_tx_hash);
    if (previous_tx_info == pool_.end())
    {
        handle_validate_(error::input_not_found,
            chain::index_list{ current_input_ });

        return;
    }

    BITCOIN_ASSERT(!(previous_tx_info->tx).is_coinbase());

    // parent_height ignored here as mempool transactions cannot be coinbase.
    handle_previous_tx(error::success, previous_tx_info->tx, 0);
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
    if (!connect_input(tx_, current_input_, previous_tx, parent_height,
        last_block_height_, value_in_))
    {
        handle_validate_(error::validate_inputs_failed, {current_input_});
        return;
    }

    // Search for double spends...
    blockchain_.fetch_spend(tx_.inputs[current_input_].previous_output,
        dispatch_.unordered_delegate(&validate_transaction::check_double_spend,
            shared_from_this(), _1));
}

void validate_transaction::check_double_spend(const std::error_code& ec)
{
    if (ec != error::unspent_output)
    {
        handle_validate_(error::double_spend, chain::index_list());
        return;
    }

    // End of connect_input checks.
    ++current_input_;
    if (current_input_ < tx_.inputs.size())
    {
        next_previous_transaction();
        return;
    }

    // current_input_ will be invalid on last pass.
    check_fees();
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
    handle_validate_(error::success, unconfirmed_);
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
    for (const auto& output: tx.outputs)
    {
        if (output.value > max_money())
            return error::output_value_overflow;

        total_output_value += output.value;
        if (total_output_value > max_money())
            return error::output_value_overflow;
    }

    if (tx.is_coinbase())
    {
        const auto& coinbase_script = tx.inputs[0].script;
        const auto coinbase_size = coinbase_script.serialized_size(false);
        if (coinbase_size < 2 || coinbase_size > 100)
            return error::invalid_coinbase_script_size;
    }
    else
    {
        for (const auto& input: tx.inputs)
            if (input.previous_output.is_null())
                return error::previous_output_null;
    }

    return error::success;
}

// Validate script consensus conformance based on flags provided.
static bool check_consensus(const chain::script& prevout_script,
    const chain::transaction& current_tx, size_t input_index,
    uint32_t options)
{
    BITCOIN_ASSERT(input_index <= max_uint32);
    BITCOIN_ASSERT(input_index < current_tx.inputs.size());
    const auto input_index32 = static_cast<uint32_t>(input_index);
    const auto bip16_enabled = ((options & validation_options::p2sh) != 0);

#ifdef WITH_CONSENSUS
    using namespace bc::consensus;
    const auto previous_output_script = prevout_script.to_data(false);
    data_chunk current_transaction = current_tx.to_data();

    const auto flags = (bip16_enabled ? verify_flags_p2sh : verify_flags_none);
    const auto result = verify_script(current_transaction.data(),
        current_transaction.size(), previous_output_script.data(),
        previous_output_script.size(), input_index32, flags);

    const auto valid = (result == verify_result::verify_result_eval_true);
#else
    // Copy the const prevout script so it can be run.
    auto previous_output_script = prevout_script;
    const auto& current_input_script = current_tx.inputs[input_index].script;

    const auto valid = chain::script::verify(current_input_script,
        previous_output_script, current_tx, input_index32, bip16_enabled);
#endif

    if (!valid)
        log_warning(LOG_VALIDATE) << "Invalid transaction ["
        << encode_hash(current_tx.hash()) << "]";

    return valid;
}

// Validate script consensus conformance, defaulting to p2sh.
static bool check_consensus(const chain::script& prevout_script,
    const chain::transaction& current_tx, size_t input_index)
{
    return check_consensus(prevout_script, current_tx, input_index,
        validation_options::p2sh);
}

// Determine if BIP16 compliance is required for this block.
static bool is_bip_16_enabled(const chain::header& header, size_t height)
{
    // Block 170060 contains an invalid BIP 16 transaction before switchover date.
    const auto bip16_enabled = header.timestamp >= bip16_switchover_timestamp;
    ////BITCOIN_ASSERT(!bip16_enabled || height >= bip16_switchover_height_mainnet);
    ////BITCOIN_ASSERT(!bip16_enabled || height >= bip16_switchover_height_testnet);
    return bip16_enabled;
}

// Validate script consensus conformance, calculating p2sh based on block/height.
bool validate_transaction::validate_consensus(const chain::script& prevout_script,
    const chain::transaction& current_tx, size_t input_index,
    const chain::header& header, const size_t height)
{
    uint32_t options = validation_options::none;
    if (is_bip_16_enabled(header, height))
        options = validation_options::p2sh;

    return check_consensus(prevout_script, current_tx, input_index, options);
}

bool validate_transaction::connect_input(const chain::transaction& tx,
    size_t current_input, const chain::transaction& previous_tx,
    size_t parent_height, size_t last_block_height, uint64_t& value_in)
{
    const auto& input = tx.inputs[current_input];
    const auto& previous_outpoint = tx.inputs[current_input].previous_output;
    if (previous_outpoint.index >= previous_tx.outputs.size())
        return false;

    const auto& previous_output = previous_tx.outputs[previous_outpoint.index];
    const auto output_value = previous_output.value;
    if (output_value > max_money())
        return false;

    if (previous_tx.is_coinbase())
    {
        const auto height_difference = last_block_height - parent_height;
        if (height_difference < coinbase_maturity)
            return false;
    }

    if (!check_consensus(previous_output.script, tx, current_input))
        return false;

    value_in += output_value;
    return value_in <= max_money();
}

bool validate_transaction::tally_fees(const chain::transaction& tx,
    uint64_t value_in, uint64_t& total_fees)
{
    const auto value_out = tx.total_output_value();

    if (value_in < value_out)
        return false;

    const auto fee = value_in - value_out;
    total_fees += fee;
    return total_fees <= max_money();
}

} // namespace blockchain
} // namespace libbitcoin
