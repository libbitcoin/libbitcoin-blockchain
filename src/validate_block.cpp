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
#include <bitcoin/blockchain/validate_block.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

// To improve readability.
#define RETURN_IF_STOPPED() \
if (stopped()) \
    return error::service_stopped

using namespace chain;

// Consensus rule change activation and enforcement parameters.
static constexpr uint8_t version_4 = 4;
static constexpr uint8_t version_3 = 3;
static constexpr uint8_t version_2 = 2;
static constexpr uint8_t version_1 = 1;

// Testnet activation parameters.
static constexpr size_t testnet_active = 51;
static constexpr size_t testnet_enforce = 75;
static constexpr size_t testnet_sample = 100;

// Mainnet activation parameters.
static constexpr size_t mainnet_active = 750;
static constexpr size_t mainnet_enforce = 950;
static constexpr size_t mainnet_sample = 1000;

// Block 173805 is the first mainnet block after date-based activation.
// Block 514 is the first testnet block after date-based activation.
static constexpr size_t mainnet_bip16_activation_height = 173805;
static constexpr size_t testnet_bip16_activation_height = 514;

// github.com/bitcoin/bips/blob/master/bip-0030.mediawiki#specification
static constexpr size_t mainnet_bip30_exception_height1 = 91842;
static constexpr size_t mainnet_bip30_exception_height2 = 91880;
static constexpr size_t testnet_bip30_exception_height1 = 0;
static constexpr size_t testnet_bip30_exception_height2 = 0;

// Max block size (1000000 bytes).
static constexpr uint32_t max_block_size = 1000000;

// Maximum signature operations per block (20000).
static constexpr uint32_t max_block_sigops = max_block_size / 50;
static_assert(max_block_sigops <= max_size_t / 2, "unsafe to add max sigops");

validate_block::validate_block(const block& block, size_t height, bool testnet,
    const config::checkpoint::list& checks, stopped_callback callback)
  : testnet_(testnet),
    height_(height),
    legacy_sigops_(0),
    activations_(script_context::none_enabled),
    minimum_version_(0),
    current_block_(block),
    checkpoints_(checks),
    stop_callback_(callback)
{
}

code validate_block::initialize_context()
{
    const auto bip30_exception_height1 = testnet_ ?
        testnet_bip30_exception_height1 :
        mainnet_bip30_exception_height1;

    const auto bip30_exception_height2 = testnet_ ?
        testnet_bip30_exception_height2 :
        mainnet_bip30_exception_height2;

    const auto bip16_activation_height = testnet_ ?
        testnet_bip16_activation_height :
        mainnet_bip16_activation_height;

    const auto active = testnet_ ? testnet_active : mainnet_active;
    const auto enforce = testnet_ ? testnet_enforce : mainnet_enforce;
    const auto sample = testnet_ ? testnet_sample : mainnet_sample;

    // Continue even if this is too small or empty (fast and simpler).
    const auto versions = preceding_block_versions(sample);

    const auto ge_4 = [](uint8_t version) { return version >= version_4; };
    const auto ge_3 = [](uint8_t version) { return version >= version_3; };
    const auto ge_2 = [](uint8_t version) { return version >= version_2; };

    const auto count_4 = std::count_if(versions.begin(), versions.end(), ge_4);
    const auto count_3 = std::count_if(versions.begin(), versions.end(), ge_3);
    const auto count_2 = std::count_if(versions.begin(), versions.end(), ge_2);

    const auto activate = [active](size_t count) { return count >= active; };
    const auto enforced = [enforce](size_t count) { return count >= enforce; };

    // version 4/3/2 enforced based on 95% of preceding 1000 mainnet blocks.
    if (enforced(count_4))
        minimum_version_ = version_4;
    else if (enforced(count_3))
        minimum_version_ = version_3;
    else if (enforced(count_2))
        minimum_version_ = version_2;
    else
        minimum_version_ = version_1;

    // bip65 is activated based on 75% of preceding 1000 mainnet blocks.
    if (activate(count_4))
        activations_ |= script_context::bip65_enabled;

    // bip66 is activated based on 75% of preceding 1000 mainnet blocks.
    if (activate(count_3))
        activations_ |= script_context::bip66_enabled;

    // bip34 is activated based on 75% of preceding 1000 mainnet blocks.
    if (activate(count_2))
        activations_ |= script_context::bip34_enabled;

    // bip30 applies to all but two mainnet blocks that violate the rule.
    if (height_ != bip30_exception_height1 &&
        height_ != bip30_exception_height2)
        activations_ |= script_context::bip30_enabled;

    // bip16 was activated with a one-time test on mainnet/testnet (~55% rule).
    if (height_ >= bip16_activation_height)
        activations_ |= script_context::bip16_enabled;

    return error::success;
}

// initialize_context must be called first (to set activations_).
bool validate_block::is_active(script_context flag) const
{
    if (!script::is_active(activations_, flag))
        return false;

    const auto version = current_block_.header.version;
    return
        (flag == script_context::bip65_enabled && version >= version_4) ||
        (flag == script_context::bip66_enabled && version >= version_3) ||
        (flag == script_context::bip34_enabled && version >= version_2);
}

// initialize_context must be called first (to set minimum_version_).
bool validate_block::is_valid_version() const
{
    return current_block_.header.version >= minimum_version_;
}

bool validate_block::stopped() const
{
    return stop_callback_();
}

// These are checks that are independent of the blockchain that can be
// validated before saving an orphan block.
code validate_block::check_block()
{
    const auto& block = current_block_;
    const auto& header = block.header;
    const auto& txs = block.transactions;

    if (txs.empty() || !txs.front().is_coinbase())
        return error::first_not_coinbase;

    RETURN_IF_STOPPED();

    if (block.serialized_size() > max_block_size)
        return error::size_limits;

    RETURN_IF_STOPPED();

    if (!header.is_valid_proof_of_work())
        return error::proof_of_work;

    RETURN_IF_STOPPED();

    if (!header.is_valid_time_stamp())
        return error::futuristic_timestamp;

    RETURN_IF_STOPPED();

    if (block.has_extra_coinbases())
        return error::extra_coinbases;

    RETURN_IF_STOPPED();

    const auto error_code = validate_transaction::check_transactions(block);

    if (error_code)
        return error_code;

    RETURN_IF_STOPPED();

    if (!block.is_distinct_transaction_set())
        return error::duplicate;

    RETURN_IF_STOPPED();

    if (block.signature_operations(false) > max_block_sigops)
        return error::too_many_sigs;

    RETURN_IF_STOPPED();

    return block.is_valid_merkle_root() ? error::success :
        error::merkle_mismatch;
}

// TODO: we could confirm block hash doesn't exist here (optimization).
code validate_block::accept_block() const
{
    const auto height = height_;
    const auto& block = current_block_;
    const auto& header = block.header;
    const auto& txs = block.transactions;

    if (txs.empty() || !txs.front().is_coinbase())
        return error::first_not_coinbase;

    RETURN_IF_STOPPED();

    // Enforce rule that the coinbase starts with serialized height.
    if (is_active(script_context::bip34_enabled) &&
        !block.is_valid_coinbase_height(height))
        return error::coinbase_height_mismatch;

    RETURN_IF_STOPPED();

    if (!block.is_final(height))
        return error::non_final_transaction;

    RETURN_IF_STOPPED();

    if (header.bits != work_required(block, testnet_))
        return error::incorrect_proof_of_work;

    RETURN_IF_STOPPED();

    if (header.timestamp <= median_time_past())
        return error::timestamp_too_early;

    RETURN_IF_STOPPED();

    // Ensure that the block passes the configured checkpoints.
    if (!config::checkpoint::validate(header.hash(), height, checkpoints_))
        return error::checkpoints_failed;

    RETURN_IF_STOPPED();

    // Reject blocks that are below the minimum version for the current height.
    if (!is_valid_version())
        return error::old_version_block;

    return error::success;
}

code validate_block::connect_block() const
{
    const auto& block = current_block_;
    const auto& txs = block.transactions;

    // Redundant with accept_block but guards this public function.
    if (txs.empty())
        return error::first_not_coinbase;

    RETURN_IF_STOPPED();

    // BIP30 is inactive for two mainnet blocks only.
    if (is_active(script_context::bip30_enabled) &&
        contains_unspent_duplicates(block))
        return error::unspent_duplicate;

    RETURN_IF_STOPPED();

    uint64_t block_fees = 0;
    size_t sigops = legacy_sigops_;
    code error_code(error::success);

    // Start at index 1 to skip coinbase.
    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    for (size_t position = 1; !error_code && position < txs.size(); ++position)
    {
        error_code = check_transaction(txs[position], position, block_fees,
            sigops);
    }
    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

    if (error_code)
        return error_code;

    // Check for excess reward claim.
    const auto earned_subsidy = block_subsidy(height_);
    const auto earned_reward = earned_subsidy + block_fees;
    const auto claimed_reward = txs.front().total_output_value();
    return claimed_reward > earned_reward ? error::coinbase_too_large :
        error::success;
}

bool validate_block::contains_unspent_duplicates(const block& block) const
{
    size_t height;
    transaction other;
    auto unspent = false;
    const auto& txs = block.transactions;

    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    for (auto tx = txs.begin(); !unspent && tx != txs.end(); ++tx)
    {
        unspent = fetch_transaction(other, height, tx->hash()) &&
            is_unspent(other);
    }
    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

    return unspent;
}

bool validate_block::is_unspent(const transaction& tx) const
{
    auto unspent = false;

    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    for (uint32_t index = 0; !unspent && index < tx.outputs.size(); ++index)
    {
        unspent = !is_output_spent({ tx.hash(), index });
    }
    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

    // BUGBUG: We cannot currently index (spent) duplicates.
    if (!unspent)
        log::error(LOG_BLOCKCHAIN)
            << "Duplicate of spent transaction [" << encode_hash(tx.hash())
            << "] valid but not supported.";

    return unspent;
}

code validate_block::check_transaction(const transaction& tx, size_t position,
    uint64_t& fees, size_t& sigops) const
{
    BITCOIN_ASSERT(!tx.is_coinbase());

    uint64_t value = 0;
    const auto error_code = check_inputs(tx, position, value, sigops);

    if (error_code)
        return error_code;

    const auto spent = tx.total_output_value();

    if (spent > value)
        return error::spend_exceeds_value;

    // Fees cannot overflow because they are less than value_out which
    // cannnot exceed value_in, which is validated against max_money as it
    // is accumulated in check_inputs, and fees + subsidy cannot overflow.
    fees += (value - spent);
    return error::success;
}

code validate_block::check_inputs(const transaction& tx, size_t position,
    uint64_t& value, size_t& sigops) const
{
    BITCOIN_ASSERT(!tx.is_coinbase());

    code error_code(error::success);
    uint32_t input_index = 0;

    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    for (; !error_code && input_index < tx.inputs.size(); ++input_index)
    {
        error_code = check_input(tx, position, input_index, value, sigops);
    }
    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

    if (error_code)
        log::warning(LOG_BLOCKCHAIN) << "Invalid input ["
            << encode_hash(tx.hash()) << ":" << input_index << "] "
            << error_code.message();

    return error_code;
}

code validate_block::check_input(const transaction& tx, size_t position,
    uint32_t input_index, uint64_t& value, size_t& sigops) const
{
    if (input_index >= tx.inputs.size())
        return error::input_not_found;

    transaction previous_tx;
    size_t previous_tx_height;
    const auto& input = tx.inputs[input_index];
    const auto& previous_point = input.previous_output;
    const auto& previous_hash = previous_point.hash;
    const auto& previous_index = previous_point.index;

    // Lookup transaction and block height of the previous output.
    if (!fetch_transaction(previous_tx, previous_tx_height, previous_hash))
        return error::input_not_found;

    RETURN_IF_STOPPED();

    const auto& previous_output = previous_tx.outputs[previous_index];
    const auto& prevout = previous_output.script;
    const auto p2sh_sigops = input.script.pay_script_hash_sigops(prevout);

    if (p2sh_sigops > max_block_sigops)
        return error::too_many_sigs;

    sigops += p2sh_sigops;

    if (sigops > max_block_sigops)
        return error::too_many_sigs;

    RETURN_IF_STOPPED();

    const auto error_code = validate_transaction::check_input(tx, input_index,
        previous_tx, previous_tx_height, height_, activations_, value);

    if (error_code)
        return error_code;

    RETURN_IF_STOPPED();

    return is_output_spent(previous_point, position, input_index) ?
        error::double_spend : error::success;
}

#undef RETURN_IF_STOPPED

} // namespace blockchain
} // namespace libbitcoin
