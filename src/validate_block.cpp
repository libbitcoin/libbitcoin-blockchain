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
static constexpr uint32_t max_block_script_sigops = max_block_size / 50;

// Value used to define retargeting range constraint.
static constexpr uint64_t retargeting_factor = 4;

// Aim for blocks every 10 mins (600 seconds).
static constexpr uint64_t target_spacing_seconds = 10 * 60;

// Target readjustment every 2 weeks (1209600 seconds).
static constexpr uint64_t target_timespan_seconds = 2 * 7 * 24 * 60 * 60;

// The target number of blocks for 2 weeks of work (2016 blocks).
static constexpr uint64_t retargeting_interval = target_timespan_seconds /
    target_spacing_seconds;

validate_block::validate_block(size_t height, const block& block, bool testnet,
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

void validate_block::initialize_context()
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
    const auto& txs = current_block_.transactions;
    const auto& header = current_block_.header;

    if (txs.empty() || current_block_.serialized_size() > max_block_size)
        return error::size_limits;

    RETURN_IF_STOPPED();

    if (!header.is_valid_proof_of_work())
        return error::proof_of_work;

    RETURN_IF_STOPPED();

    if (!header.is_valid_time_stamp())
        return error::futuristic_timestamp;

    RETURN_IF_STOPPED();

    if (!txs.front().is_coinbase())
        return error::first_not_coinbase;

    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        if (tx->is_coinbase())
            return error::extra_coinbases;

        RETURN_IF_STOPPED();
    }

    for (const auto& tx: txs)
    {
        // Basic checks that are independent of the mempool.
        const auto ec = validate_transaction::check_transaction(tx);

        if (ec)
            return ec;

        RETURN_IF_STOPPED();
    }

    RETURN_IF_STOPPED();

    if (!current_block_.is_distinct_transaction_set())
        return error::duplicate;

    RETURN_IF_STOPPED();

    // HACK: instead cache legacy signature operations on the block itself.
    legacy_sigops_ = current_block_.signature_operations(false);

    if (legacy_sigops_ > max_block_script_sigops)
        return error::too_many_sigs;

    RETURN_IF_STOPPED();

    return current_block_.generate_merkle_root() == header.merkle ?
        error::success : error::merkle_mismatch;
}

// TODO: we could confirm block hash doesn't exist here (optimization).
code validate_block::accept_block() const
{
    const auto& header = current_block_.header;
    const auto& txs = current_block_.transactions;

    if (header.bits != work_required(testnet_))
        return error::incorrect_proof_of_work;

    RETURN_IF_STOPPED();

    if (header.timestamp <= median_time_past())
        return error::timestamp_too_early;

    RETURN_IF_STOPPED();

    // Txs must be final when included in a block.
    for (const auto& tx: txs)
    {
        if (!tx.is_final(height_, header.timestamp))
            return error::non_final_transaction;

        RETURN_IF_STOPPED();
    }

    // Ensure that the block passes checkpoints.
    // This is both DOS protection and performance optimization for sync.
    if (!config::checkpoint::validate(header.hash(), height_, checkpoints_))
        return error::checkpoints_failed;

    RETURN_IF_STOPPED();

    // Reject blocks that are below the minimum version for the current height.
    if (!is_valid_version())
        return error::old_version_block;

    RETURN_IF_STOPPED();

    // Enforce rule that the coinbase starts with serialized height.
    if (is_active(script_context::bip34_enabled) &&
        !current_block_.is_valid_coinbase_height(height_))
        return error::coinbase_height_mismatch;

    return error::success;
}

uint32_t validate_block::work_required(bool is_testnet) const
{
    if (height_ == 0)
        return max_work_bits;

    const auto is_retarget_height = [](size_t height)
    {
        return height % retargeting_interval == 0;
    };

    if (is_retarget_height(height_))
    {
        // This is the total time it took for the last 2016 blocks.
        const auto actual = actual_time_span(retargeting_interval);

        // Now constrain the time between an upper and lower bound.
        const auto constrained = range_constrain(actual,
            target_timespan_seconds / retargeting_factor,
            target_timespan_seconds * retargeting_factor);

        // TODO: technically this set_compact can fail.
        hash_number retarget;
        retarget.set_compact(previous_block_bits());
        retarget *= constrained;
        retarget /= target_timespan_seconds;

        // TODO: This should be statically-initialized.
        hash_number maximum;
        maximum.set_compact(max_work_bits);

        if (retarget > maximum)
            retarget = maximum;

        return retarget.compact();
    }

    if (!is_testnet)
        return previous_block_bits();

    // This section is testnet in not-retargeting scenario.
    // ------------------------------------------------------------------------

    const auto max_time_gap = fetch_block(height_ - 1).timestamp + 2 * 
        target_spacing_seconds;

    if (current_block_.header.timestamp > max_time_gap)
        return max_work_bits;

    const auto last_non_special_bits = [this, is_retarget_height]()
    {
        header previous_block;
        auto previous_height = height_;

        // TODO: this is very suboptimal, cache the set of change points.
        // Loop backwards until we find a difficulty change point,
        // or we find a block which does not have max_bits (is not special).
        while (!is_retarget_height(previous_height))
        {
            previous_block = fetch_block(--previous_height);

            // Test for non-special block.
            if (previous_block.bits != max_work_bits)
                break;
        }

        return previous_block.bits;
    };

    return last_non_special_bits();

    // ------------------------------------------------------------------------
}

code validate_block::connect_block() const
{
    // BIP30 is inactive for two mainnet blocks only.
    if (is_active(script_context::bip30_enabled) &&
        contains_unspent_duplicates())
        return error::unspent_duplicate;

    uint64_t block_fees = 0;
    size_t sigops = legacy_sigops_;
    code error_code(error::success);
    const auto& txs = current_block_.transactions;

    //////////////////////////// TODO: parallelize. ///////////////////////////
    // Start at index 1 to skip coinbase.
    for (size_t index = 1; !error_code && index < txs.size(); ++index)
        error_code = check_transaction(txs[index], index, block_fees, sigops);
    ///////////////////////////////////////////////////////////////////////////

    if (error_code)
        return error_code;

    // Check for excess reward claim.
    const auto earned_subsidy = block_subsidy(height_);
    const auto earned_reward = earned_subsidy + block_fees;
    const auto claimed_reward = txs.front().total_output_value();
    return claimed_reward > earned_reward ? error::coinbase_too_large :
        error::success;
}

bool validate_block::contains_unspent_duplicates() const
{
    size_t height;
    transaction other;
    auto unspent = false;
    const auto& txs = current_block_.transactions;

    //////////////////////////// TODO: parallelize. ///////////////////////////
    for (auto tx = txs.begin(); !unspent && tx != txs.end(); ++tx)
        unspent = fetch_transaction(other, height, tx->hash()) &&
            is_unspent(other);
    ///////////////////////////////////////////////////////////////////////////

    return unspent;
}

bool validate_block::is_unspent(const transaction& tx) const
{
    auto unspent = false;

    //////////////////////////// TODO: parallelize. ///////////////////////////
    for (uint32_t index = 0; !unspent && index < tx.outputs.size(); ++index)
        unspent = !is_output_spent({ tx.hash(), index });
    ///////////////////////////////////////////////////////////////////////////

    // BUGBUG: We cannot currently index (spent) duplicates.
    if (!unspent)
        log::error(LOG_BLOCKCHAIN)
            << "Duplicate of spent transaction [" << encode_hash(tx.hash())
            << "] valid but not supported.";

    return unspent;
}

code validate_block::check_transaction(const transaction& tx, size_t index,
    size_t& fees, size_t& sigops) const
{
    uint64_t value = 0;
    const auto error_code = check_inputs(tx, index, value, sigops);

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

code validate_block::check_inputs(const transaction& tx,
    size_t index_in_block, uint64_t& value, size_t& sigops) const
{
    BITCOIN_ASSERT(!tx.is_coinbase());
    code error_code(error::success);
    size_t index = 0;

    //////////////////////////// TODO: parallelize. ///////////////////////////
    for (; !error_code && index < tx.inputs.size(); ++index)
        error_code = check_input(tx, index_in_block, index, value, sigops);
    ///////////////////////////////////////////////////////////////////////////

    if (error_code)
        log::warning(LOG_BLOCKCHAIN) << "Invalid input ["
            << encode_hash(tx.hash()) << ":" << index << "] "
            << error_code.message();

    return error_code;
}

code validate_block::check_input(const transaction& tx, size_t index_in_block,
    size_t input_index, uint64_t& value, size_t& sigops) const
{
    BITCOIN_ASSERT(input_index < tx.inputs.size());

    size_t previous_height;
    transaction previous_tx;
    const auto& input = tx.inputs[input_index];
    const auto& previous_point = input.previous_output;

    // Lookup previous output.
    // This searches the blockchain and then the orphan pool up to and
    // including the current (orphan) block and excluding blocks above fork.
    if (!fetch_transaction(previous_tx, previous_height, previous_point.hash))
    {
        log::warning(LOG_BLOCKCHAIN)
            << "Failure fetching input transaction ["
            << encode_hash(previous_point.hash) << "]";
        return error::input_not_found;
    }

    RETURN_IF_STOPPED();

    const auto& script = previous_tx.outputs[previous_point.index].script;
    auto error_code = check_sigops(script, input.script, sigops);

    if (error_code)
        return error_code;

    RETURN_IF_STOPPED();

    error_code = validate_transaction::check_input(tx, input_index,
        previous_tx, height_, previous_height, value, activations_);

    if (error_code)
        return error_code;

    RETURN_IF_STOPPED();

    return is_output_spent(previous_point, index_in_block, input_index) ?
        error::double_spend : error::success;
}

code validate_block::check_sigops(const script& output, const script& input,
    size_t& sigops) const
{
    const auto more = output.pay_to_script_hash_signature_operations(input);

    // Guard against overflow.
    if (sigops > max_size_t - more)
        return error::too_many_sigs;

    // Add sigops from p2sh payments.
    sigops += more;

    return sigops > max_block_script_sigops ? error::too_many_sigs :
        error::success;
}

#undef RETURN_IF_STOPPED

} // namespace blockchain
} // namespace libbitcoin
