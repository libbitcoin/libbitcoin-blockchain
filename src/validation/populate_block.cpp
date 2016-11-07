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
#include <bitcoin/blockchain/validation/populate_block.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace std::placeholders;

#define NAME "populate_block"

// These values should not be used, but are helpful in the debugger.
static constexpr uint32_t unspecified = 0xbaadf00d;

// Constant for log report calculations.
static constexpr size_t micro_per_milliseconds = 1000;

// Database access is limited to:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

// TODO: allow priority pool to be empty and fall back the network pool:
// dispatch_(priority_pool.size() == 0 ? network_pool : priority_pool)
populate_block::populate_block(threadpool& priority_pool,
    const fast_chain& chain, const settings& settings)
  : stopped_(false),
    priority_threads_(priority_pool.size()),
    use_testnet_rules_(settings.use_testnet_rules),
    checkpoints_(config::checkpoint::sort(settings.checkpoints)),
    dispatch_(priority_pool, NAME "_dispatch"),
    fast_chain_(chain)
{
}

// Stop sequence.
//-----------------------------------------------------------------------------

void populate_block::stop()
{
    stopped_.store(true);
}

bool populate_block::stopped() const
{
    return stopped_.load();
}

// Populate chain state.
//-----------------------------------------------------------------------------
// TODO: move to another class/file (populate_chain_state).

bool populate_block::get_bits(uint32_t& out_bits, size_t height,
    fork::const_ptr fork) const
{
    // fork returns false only if the height is out of range.
    return fork->get_bits(out_bits, height) ||
        fast_chain_.get_bits(out_bits, height);
}

bool populate_block::get_version(uint32_t& out_version, size_t height,
    fork::const_ptr fork) const
{
    // fork returns false only if the height is out of range.
    return fork->get_version(out_version, height) ||
        fast_chain_.get_version(out_version, height);
}

bool populate_block::get_timestamp(uint32_t& out_timestamp, size_t height,
    fork::const_ptr fork) const
{
    // fork returns false only if the height is out of range.
    return fork->get_timestamp(out_timestamp, height) ||
        fast_chain_.get_timestamp(out_timestamp, height);
}

bool populate_block::populate_bits(chain_state::data& data,
    const chain_state::map& map, fork::const_ptr fork) const
{
    auto& bits = data.bits.ordered;
    bits.resize(map.bits.count);
    auto height = map.bits.high - map.bits.count;

    for (auto& bit: bits)
        if (!get_bits(bit, ++height, fork))
            return false;

    return true;
}

bool populate_block::populate_versions(chain_state::data& data,
    const chain_state::map& map, fork::const_ptr fork) const
{
    auto& versions = data.version.unordered;
    versions.resize(map.version.count);
    auto height = map.version.high - map.version.count;

    for (auto& version: versions)
        if (!get_version(version, ++height, fork))
            return false;

    return true;
}

bool populate_block::populate_timestamps(chain_state::data& data,
    const chain_state::map& map, fork::const_ptr fork) const
{
    data.timestamp.retarget = unspecified;
    auto& timestamps = data.timestamp.ordered;
    timestamps.resize(map.timestamp.count);
    auto height = map.timestamp.high - map.timestamp.count;

    for (auto& timestamp: timestamps)
        if (!get_timestamp(timestamp, ++height, fork))
            return false;

    if (!get_timestamp(data.timestamp.self, map.timestamp_self, fork))
        return false;

    // Retarget not required if timestamp_retarget is zero.
    return map.timestamp_retarget == 0 ||
        get_timestamp(data.timestamp.retarget, map.timestamp_retarget, fork);
}

// TODO: populate data.activated by caching full activation height.
// The height must be tied to block push/pop and invalidated on failure.
void populate_block::populate_chain_state(fork::const_ptr fork,
    size_t index) const
{
    BITCOIN_ASSERT(index < fork->size());

    chain_state::data data;
    data.enabled = false;
    data.testnet = use_testnet_rules_;
    data.height = fork->height_at(index);
    data.hash = fork->block_at(index)->hash();
    auto map = chain_state::get_map(data.height, data.enabled, data.testnet);

    // At most 11 redundant mainnet header queries, so we don't combine them.
    // Cache-based construction of the data set will eliminate most redundancy.
    if (populate_bits(data, map, fork) &&
        populate_versions(data, map, fork) &&
        populate_timestamps(data, map, fork))
    {
        auto& state = fork->block_at(index)->validation.state;
        state = std::make_shared<chain_state>(std::move(data), checkpoints_);
    }
}

// Populate input sets utility.
//-----------------------------------------------------------------------------

void populate_block::populate_input_sets(fork::const_ptr fork,
    size_t index) const
{
    BITCOIN_ASSERT(index < fork->size());
    const auto block = fork->block_at(index);

    // TODO: balance by sigop count when used for script validation.
    // Balanced by input count, which is optimal for data population.
    block->validation.sets = block->to_input_sets(priority_threads_, false);
}

// Populate block state sequence.
//-----------------------------------------------------------------------------

void populate_block::populate(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    populate_input_sets(fork, index);
    populate_chain_state(fork, index);
    populate_transactions(fork, index, handler);
}

void populate_block::populate_transactions(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    const auto block = fork->block_at(index);
    const auto& txs = block->transactions();
    const auto sets = block->validation.sets;

    populate_coinbase(block);

    // Sets will be empty if there is only a coinbase tx.
    if (sets->empty())
    {
        handler(error::success);
        return;
    }

    // Populate tx data and verify not unspent duplicate.
    //*************************************************************************
    // CONSENSUS: Coinbase prevouts are null but the tx duplicate check must
    // apply to coinbase txs as well, so we cannot skip coinbases here.
    //*************************************************************************
    for (auto tx = txs.begin(); tx != txs.end(); ++tx)
    {
        populate_transaction(fork->height(), *tx);
        populate_transaction(fork, index, *tx);
    }

    const result_handler join_handler = synchronize(handler, sets->size(),
        NAME "_populate");

    ////// Sequential implementation.
    ////for (size_t set = 0; set < sets->size(); ++set)
    ////    populate_inputs(fork, index, sets, set, join_handler);

    // Concurrent implementation.
    for (size_t set = 0; set < sets->size(); ++set)
        dispatch_.concurrent(&populate_block::populate_inputs,
            this, fork, index, sets, set, join_handler);
}

// Initialize the coinbase input for subsequent validation.
void populate_block::populate_coinbase(block_const_ptr block) const
{
    const auto& txs = block->transactions();
    BITCOIN_ASSERT(!txs.empty() && txs.front().is_coinbase());

    // A coinbase tx guarnatees exactly one input.
    const auto& input = txs.front().inputs().front();
    auto& prevout = input.previous_output().validation;

    // A coinbase input cannot be a double spend since it originates coin.
    prevout.spent = false;

    // A coinbase is only valid within a block and input is confirmed if valid.
    prevout.confirmed = true;

    // A coinbase input has no previous output.
    prevout.cache = chain::output{};

    // A coinbase input does not spend an output so is itself always mature.
    prevout.height = output_point::validation::not_specified;
}

// Returns false only when transaction is duplicate on chain.
void populate_block::populate_transaction(size_t fork_height,
    const chain::transaction& tx) const
{
    tx.validation.duplicate = fast_chain_.get_is_unspent_transaction(
        tx.hash(), fork_height);
}

// Returns false only when transaction is duplicate on fork.
void populate_block::populate_transaction(fork::const_ptr fork, size_t index,
    const chain::transaction& tx) const
{
    if (!tx.validation.duplicate)
        fork->populate_tx(index, tx);
}

void populate_block::populate_inputs(fork::const_ptr fork, size_t index,
    transaction::sets_const_ptr input_sets, size_t sets_index,
    result_handler handler) const
{
    BITCOIN_ASSERT(!input_sets->empty());
    BITCOIN_ASSERT(sets_index < input_sets->size());

    code ec(error::success);
    const auto fork_height = fork->height();
    const auto& sets = (*input_sets)[sets_index];

    for (auto& set: sets)
    {
        BITCOIN_ASSERT(!set.tx.is_coinbase());
        BITCOIN_ASSERT(set.input_index < set.tx.inputs().size());
        const auto& input = set.tx.inputs()[set.input_index];

        if (stopped())
        {
            ec = error::service_stopped;
            break;
        }

        populate_prevout(fork_height, input.previous_output());
        populate_prevout(fork, index, input.previous_output());
    }

    handler(ec);
}

void populate_block::populate_prevout(size_t fork_height,
    const output_point& outpoint) const
{
    // The previous output will be cached on the input's outpoint.
    auto& prevout = outpoint.validation;

    prevout.spent = false;
    prevout.confirmed = false;
    prevout.cache = chain::output{};
    prevout.height = output_point::validation::not_specified;

    // If the input is a coinbase there is no prevout to populate.
    if (outpoint.is_null())
        return;

    size_t height;
    size_t position;

    // Get the script, value and spender height (if any) for the prevout.
    // The output (prevout.cache) is populated only if the return is true.
    if (!fast_chain_.get_output(prevout.cache, height, position, outpoint,
        fork_height))
        return;

    // Set height only if prevout is coinbase (first position tx is coinbase).
    if (position == 0)
        prevout.height = height;

    // The output is spent only if by a spend at or below the fork height.
    const auto spend_height = prevout.cache.validation.spender_height;

    if ((spend_height <= fork_height) &&
        (spend_height != output::validation::not_spent))
    {
        prevout.spent = true;
        prevout.confirmed = true;
        prevout.cache = chain::output{};
        return;
    }
}

void populate_block::populate_prevout(fork::const_ptr fork, size_t index,
    const output_point& outpoint) const
{
    if (!outpoint.validation.spent)
        fork->populate_spent(index, outpoint);

    // We continue even if prevout is spent.

    if (!outpoint.validation.cache.is_valid())
        fork->populate_prevout(index, outpoint);
}

} // namespace blockchain
} // namespace libbitcoin
