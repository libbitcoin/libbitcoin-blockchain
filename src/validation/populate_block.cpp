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

// This value should never be read, but may be useful in debugging.
static constexpr uint32_t unspecified = max_uint32;

// Database access is limited to:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

populate_block::populate_block(threadpool& priority_pool,
    const fast_chain& chain, const settings& settings)
  : stopped_(false),
    configured_forks_(settings.enabled_forks),
    checkpoints_(config::checkpoint::sort(settings.checkpoints)),
    priority_dispatch_(priority_pool, NAME "_dispatch"),
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

bool populate_block::get_block_hash(hash_digest& out_hash, size_t height,
    fork::const_ptr fork) const
{
    return fork->get_block_hash(out_hash, height) ||
        fast_chain_.get_block_hash(out_hash, height);
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

    if (!get_version(data.version.self, map.version_self, fork))
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

    // Retarget not required if timestamp_retarget is unrequested.
    return map.timestamp_retarget == chain_state::map::unrequested ||
        get_timestamp(data.timestamp.retarget, map.timestamp_retarget, fork);
}

bool populate_block::populate_checkpoint(chain_state::data& data,
    const chain_state::map& map, fork::const_ptr fork) const
{
    if (map.allowed_duplicates_height == chain_state::map::unrequested)
    {
        // The allowed_duplicates_hash must be null_hash if unrequested.
        data.allowed_duplicates_hash = null_hash;
        return true;
    }

    return get_block_hash(data.allowed_duplicates_hash,
        map.allowed_duplicates_height, fork);
}

void populate_block::populate_chain_state(fork::const_ptr fork) const
{
    const auto block = fork->top();
    const auto height = fork->top_height();
    BITCOIN_ASSERT(block);

    chain_state::data data;
    data.height = height;
    data.hash = block->hash();

    // TODO: generate from cache using preceding block's map and data.
    // Construct a map to inform chain state data population.
    const auto map = chain_state::get_map(data.height, checkpoints_,
        configured_forks_);

    // Populate state and attach to block member.
    if (populate_bits(data, map, fork) &&
        populate_versions(data, map, fork) &&
        populate_timestamps(data, map, fork) &&
        populate_checkpoint(data, map, fork))
    {
        block->validation.state = std::make_shared<chain_state>(
            std::move(data), checkpoints_, configured_forks_);
    }
}

// Populate block state sequence.
//-----------------------------------------------------------------------------

void populate_block::populate_block_state(fork::const_ptr fork,
    result_handler&& handler) const
{
    const auto block = fork->top();
    BITCOIN_ASSERT(block);

    // Populate chain state if not already populated, always required.
    if (!block->validation.state)
        populate_chain_state(fork);

    BITCOIN_ASSERT(block->validation.state);
    const auto state = block->validation.state;

    // Return if this blocks is under a checkpoint, block state not requried.
    if (state->is_under_checkpoint())
    {
        handler(error::success);
        return;
    }

    populate_coinbase(block);

    //*************************************************************************
    // CONSENSUS: Satoshi implemented this change in Nov 2015. This was a hard 
    // fork that will produce catostrophic results in the case of a hash
    // collision. Unspent duplicate check has cost but should not be skipped.
    //*************************************************************************
    if (!state->is_enabled(machine::rule_fork::allowed_duplicates))
    {
        //*********************************************************************
        // CONSENSUS: Coinbase prevouts are null but the tx duplicate check
        // must apply to coinbase txs as well, so cannot skip coinbases here.
        //*********************************************************************
        // TODO: paralellize by transaction.
        for (auto& tx: block->transactions())
        {
            populate_transaction(fork->height(), tx);
            populate_transaction(fork, tx);
        }
    }

    const auto non_coinbase_inputs = block->total_inputs(false);

    // Return if there are no non-coinbase inputs to validate.
    if (non_coinbase_inputs == 0)
    {
        handler(error::success);
        return;
    }

    const auto threads = priority_dispatch_.size();
    const auto buckets = std::min(threads, non_coinbase_inputs);
    const auto join_handler = synchronize(std::move(handler), buckets,
        NAME "_populate");

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        priority_dispatch_.concurrent(&populate_block::populate_inputs,
            this, fork, bucket, buckets, join_handler);
}

// Initialize the coinbase input for subsequent validation.
void populate_block::populate_coinbase(block_const_ptr block) const
{
    const auto& txs = block->transactions();
    BITCOIN_ASSERT(!txs.empty() && txs.front().is_coinbase());

    // A coinbase tx guarantees exactly one input.
    const auto& input = txs.front().inputs().front();
    auto& prevout = input.previous_output().validation;

    // A coinbase input cannot be a double spend since it originates coin.
    prevout.spent = false;

    // A coinbase is only valid within a block and input is confirmed if valid.
    prevout.confirmed = true;

    // A coinbase input has no previous output.
    prevout.cache = chain::output{};

    // A coinbase input does not spend an output so is itself always mature.
    prevout.height = output_point::validation_type::not_specified;
}

void populate_block::populate_transaction(size_t fork_height,
    const chain::transaction& tx) const
{
    tx.validation.duplicate = fast_chain_.get_is_unspent_transaction(
        tx.hash(), fork_height);
}

void populate_block::populate_transaction(fork::const_ptr fork,
    const chain::transaction& tx) const
{
    if (!tx.validation.duplicate)
        fork->populate_tx(tx);
}

void populate_block::populate_inputs(fork::const_ptr fork,
    size_t bucket, size_t buckets, result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    code ec(error::success);
    const auto block = fork->top();
    const auto fork_height = fork->top_height();
    const auto& txs = block->transactions();
    size_t position = 0;

    // Must skip coinbase here as it is already accounted for.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        const auto& inputs = tx->inputs();

        // TODO: eliminate the wasteful iterations by using smart step.
        for (size_t input_index = 0; input_index < inputs.size();
            ++input_index, ++position)
        {
            if (position % buckets != bucket)
                continue;

            const auto& input = inputs[input_index];
            const auto& output = input.previous_output();

            populate_prevout(fork_height, output);
            populate_prevout(fork, output);
        }
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
    prevout.height = output_point::validation_type::not_specified;

    // If the input is a coinbase there is no prevout to populate.
    if (outpoint.is_null())
        return;

    size_t output_height;
    bool output_coinbase;

    // Get the script, value and spender height (if any) for the prevout.
    // The output (prevout.cache) is populated only if the return is true.
    if (!fast_chain_.get_output(prevout.cache, output_height, output_coinbase,
        outpoint, fork_height))
        return;

    //*************************************************************************
    // CONSENSUS: The genesis block coinbase may not be spent. This is the
    // consequence of satoshi not including it in the utxo set for block
    // database initialization. Only he knows why, probably an oversight.
    //*************************************************************************
    if (output_height == 0)
        return;

    // Set height only if the prevout is a coinbase tx (for maturity).
    if (output_coinbase)
        prevout.height = output_height;

    // The output is spent only if by a spend at or below the fork height.
    const auto spend_height = prevout.cache.validation.spender_height;

    // The previous output has already been spent.
    if ((spend_height <= fork_height) &&
        (spend_height != output::validation::not_spent))
    {
        prevout.spent = true;
        prevout.confirmed = true;
        prevout.cache = chain::output{};
    }
}

void populate_block::populate_prevout(fork::const_ptr fork,
    const output_point& outpoint) const
{
    if (!outpoint.validation.spent)
        fork->populate_spent(outpoint);

    // We continue even if prevout is spent.

    if (!outpoint.validation.cache.is_valid())
        fork->populate_prevout(outpoint);
}

} // namespace blockchain
} // namespace libbitcoin
