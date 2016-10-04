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
    auto start = map.bits.low;
    auto& bits = data.bits.ordered;
    bits.resize(map.bits.high - start + 1u);

    for (auto& bit: bits)
        if (!get_bits(bit, start++, fork))
            return false;

    return true;
}

bool populate_block::populate_versions(chain_state::data& data,
    const chain_state::map& map, fork::const_ptr fork) const
{
    auto start = map.version.low;
    auto& versions = data.version.unordered;
    versions.resize(map.version.high - start + 1u);

    for (auto& bit: versions)
        if (!get_version(bit, start++, fork))
            return false;

    return true;
}

bool populate_block::populate_timestamps(chain_state::data& data,
    const chain_state::map& map, fork::const_ptr fork) const
{
    auto start = map.timestamp.low;
    auto& timestamps = data.timestamp.ordered;
    timestamps.resize(map.timestamp.high - start + 1u);

    for (auto& timestamp: timestamps)
        if (!get_timestamp(timestamp, start++, fork))
            return false;

    // Won't be used, just looks cool in the debugger.
    data.timestamp.self = 0xbaadf00d;
    data.timestamp.retarget = 0xbaadf00d;

    // Additional self requirement is signaled by self != high.
    if (map.timestamp_self != map.timestamp.high &&
        !get_timestamp(data.timestamp.self, 
            map.timestamp_self, fork))
            return false;

    // Additional retarget requirement is signaled by retarget != high.
    if (map.timestamp_retarget != map.timestamp.high &&
        !get_timestamp(data.timestamp.retarget, 
            map.timestamp_retarget, fork))
            return false;

    return true;
}

// TODO: populate data.activated by caching full activation height.
// The hight must be tied to block push/pop and invalidated on failure.
void populate_block::populate_chain_state(fork::const_ptr fork, size_t index) const
{
    BITCOIN_ASSERT(index < fork->size());

    chain_state::data data;
    data.enabled = false;
    data.testnet = use_testnet_rules_;
    data.height = fork->height_at(index);
    auto map = chain_state::get_map(data.height, data.enabled, data.testnet);

    // There are only 11 redundant queries on mainnet, so we don't combine.
    // cache-based construction of the data set will eliminate most redundancy.
    if (populate_bits(data, map, fork) &&
        populate_versions(data, map, fork) &&
        populate_timestamps(data, map, fork))
    {
        auto& state = fork->block_at(index)->validation.state;
        state = std::make_shared<chain_state>(std::move(data), checkpoints_);
    }
}

void populate_block::populate_input_sets(fork::const_ptr fork,
    size_t index) const
{
    BITCOIN_ASSERT(index < fork->size());

    const auto block = fork->block_at(index);
    block->validation.sets = block->to_input_sets(priority_threads_, false);
}

// Populate block state sequence.
//-----------------------------------------------------------------------------
// Guarantees handler is invoked on a new thread.

void populate_block::populate(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    ////auto start_time = asio::steady_clock::now();
    ////report(block, start_time, "chainstat");

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

    const result_handler complete_handler =
        std::bind(&populate_block::handle_populate,
            this, _1, block, asio::steady_clock::now(), handler);

    populate_coinbase(block);

    // Sets will be empty if there is only a coinbase tx.
    if (sets->empty())
    {
        complete_handler(error::success);
        return;
    }

    // Populate non-coinbase tx data.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        populate_transaction(*tx);
        populate_transaction(fork, index, *tx);
    }

    const result_handler join_handler = synchronize(complete_handler,
        sets->size(), NAME "_populate");

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
    prevout.cache.reset();

    // A coinbase input does not spend an output so is itself always mature.
    prevout.height = output_point::validation::not_specified;
}

// Returns false only when transaction is duplicate on chain.
void populate_block::populate_transaction(const chain::transaction& tx) const
{
    // BUGBUG: this is overly restrictive, see BIP30.
    tx.validation.duplicate = fast_chain_.get_is_unspent_transaction(
        tx.hash());
}

// Returns false only when transaction is duplicate on fork.
void populate_block::populate_transaction(fork::const_ptr fork, size_t index,
    const chain::transaction& tx) const
{
    // Distinctness is a static check so this looks only below the index.
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
        if (stopped())
        {
            ec = error::service_stopped;
            break;
        }

        BITCOIN_ASSERT(!set.tx.is_coinbase());
        BITCOIN_ASSERT(set.input_index < set.tx.inputs().size());
        const auto& input = set.tx.inputs()[set.input_index];

        if (!populate_spent(fork_height, input.previous_output()))
        {
            // This is the only early terminate (database integrity error).
            ec = error::operation_failed;
            break;
        }

        populate_spent(fork, index, input.previous_output());
        populate_prevout(fork_height, input.previous_output());
        populate_prevout(fork, index, input.previous_output());
    }

    handler(ec);
}

// Returns false only when database operation fails.
void populate_block::populate_spent(fork::const_ptr fork, size_t index,
    const output_point& outpoint) const
{
    if (!outpoint.validation.spent)
        fork->populate_spent(index, outpoint);
}

// Returns false only when database operation fails.
bool populate_block::populate_spent(size_t fork_height,
    const output_point& outpoint) const
{
    size_t spender_height;
    hash_digest spender_hash;
    auto& prevout = outpoint.validation;

    // Confirmed state matches spend state (unless tx pool validation).
    prevout.confirmed = false;

    // Determine if the prevout is spent by a confirmed input.
    prevout.spent = fast_chain_.get_spender_hash(spender_hash, outpoint);

    // Either the prevout is unspent, spent in fork, the outpoint is invalid.
    if (!prevout.spent)
        return true;

    // It is a store failure it the spender transaction is not found.
    if (!fast_chain_.get_transaction_height(spender_height, spender_hash))
        return false;

    // Unspend the prevout if it is above the fork.
    prevout.spent = spender_height <= fork_height;

    // All block spends are confirmed spends.
    prevout.confirmed = prevout.spent;
    return true;
}

void populate_block::populate_prevout(fork::const_ptr fork, size_t index,
    const output_point& outpoint) const
{
    if (!outpoint.validation.cache.is_valid())
        fork->populate_prevout(index, outpoint);
}

void populate_block::populate_prevout(size_t fork_height,
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

    ///////////////////////////////////////////////////////////////////////////
    // We continue even if prevout spent and/or missing.
    ///////////////////////////////////////////////////////////////////////////

    // Get the script and value for the prevout.
    if (!fast_chain_.get_output(prevout.cache, height, position, outpoint))
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

void populate_block::handle_populate(const code& ec, block_const_ptr block,
    asio::time_point start_time, result_handler handler) const
{
    const auto token = code(ec) ? "UNPOPULATED" : "populated";
    report(block, start_time, token);
    handler(ec);
}

// Utility.
//-----------------------------------------------------------------------------

void populate_block::report(block_const_ptr block, asio::time_point start_time,
    const std::string& token)
{
    BITCOIN_ASSERT(block->validation.state);
    static constexpr size_t micro_per_milliseconds = 1000;
    const auto delta = asio::steady_clock::now() - start_time;
    const auto elapsed = std::chrono::duration_cast<asio::microseconds>(delta);
    const auto micro_per_block = static_cast<float>(elapsed.count());
    const auto micro_per_input = micro_per_block / block->total_inputs();
    const auto milli_per_block = micro_per_block / micro_per_milliseconds;
    const auto transactions = block->transactions().size();
    const auto next_height = block->validation.state->height();

    log::info(LOG_BLOCKCHAIN)
        << "Block [" << next_height << "] " << token << " (" << transactions
        << ") txs in (" << milli_per_block << ") ms or (" << micro_per_input
        << ") μs/input";
}

} // namespace blockchain
} // namespace libbitcoin
