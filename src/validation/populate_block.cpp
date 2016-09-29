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

populate_block::populate_block(threadpool& pool, const simple_chain& chain,
    const settings& settings)
  : stopped_(false),
    use_testnet_rules_(settings.use_testnet_rules),
    checkpoints_(config::checkpoint::sort(settings.checkpoints)),
    chain_(chain),
    dispatch_(pool, NAME "_dispatch")
{
}

// Stop sequence (thread safe).
//-----------------------------------------------------------------------------

void populate_block::stop()
{
    stopped_.store(true);
}

bool populate_block::stopped() const
{
    return stopped_.load();
}

// Populate (chain and block) state sequence.
//-----------------------------------------------------------------------------

void populate_block::populate(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    BITCOIN_ASSERT(!fork->empty());
    BITCOIN_ASSERT(index < fork->size());

    const auto block = fork->block_at(index);
    const auto height = fork->height_at(index);

    block->validation.state = create_chain_state(height);
    BITCOIN_ASSERT(block->validation.state);

    populate_transactions(fork, index, handler);
}

chain_state::ptr populate_block::create_chain_state(size_t height) const
{
    auto state = std::make_shared<chain_state>(use_testnet_rules_,
        checkpoints_);

    ///////////////////////////////////////////////////////////////////////////
    // TODO: populate state for chain state.
    chain_state::versions history(state->sample_size);
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    // TODO: enable last two block validation rules that require chain state.
    state->set_context(height, history);
    ///////////////////////////////////////////////////////////////////////////

    return state;
}

void populate_block::populate_transactions(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    const auto block = fork->block_at(index);
    const auto& txs = block->transactions;

    const result_handler complete_handler =
        std::bind(&populate_block::handle_populate,
            this, _1, block, asio::steady_clock::now(), handler);

    populate_coinbase(block);

    if (txs.size() < 2)
    {
        // We must complete on a new thread.
        dispatch_.concurrent(complete_handler, error::success);
        return;
    }

    const result_handler join_handler = synchronize(complete_handler,
        txs.size() - 1, NAME "_populate");

    // skip coinbase.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        populate_transaction(*tx);
        populate_transaction(fork, index, *tx);

        // We must complete on a new thread.
        dispatch_.concurrent(&populate_block::populate_inputs,
            this, fork, index, std::ref(*tx), join_handler);
    }
}

// Initialize the coinbase input for subsequent validation.
void populate_block::populate_coinbase(block_const_ptr block) const
{
    const auto& txs = block->transactions;
    BITCOIN_ASSERT(!txs.empty() && txs.front().is_coinbase());

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

// Returns false only when transaction is duplicate on chain.
void populate_block::populate_transaction(const chain::transaction& tx) const
{
    // BUGBUG: this is overly restrictive, see BIP30.
    tx.validation.duplicate = chain_.get_is_unspent_transaction(tx.hash());
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
    const transaction& tx, result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    const auto fork_height = fork->height();

    for (auto& input: tx.inputs)
    {
        if (!populate_spent(fork_height, input.previous_output))
        {
            // This is the only early terminate (database integrity error).
            handler(error::operation_failed);
            return;
        }

        populate_spent(fork, index, input.previous_output);
        populate_prevout(fork_height, input.previous_output);
        populate_prevout(fork, index, input.previous_output);
    }

    handler(error::success);
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
    const auto transactions = block->transactions.size();
    const auto next_height = block->validation.state->next_height();

    log::info(LOG_BLOCKCHAIN)
        << "Block [" << next_height << "] " << token << " (" << transactions
        << ") txs in (" << milli_per_block << ") ms or (" << micro_per_input
        << ") μs/input";
}

} // namespace blockchain
} // namespace libbitcoin
