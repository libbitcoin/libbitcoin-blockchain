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
#include <bitcoin/blockchain/populate/populate_block.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;

#define NAME "populate_block"

// Database access is limited to:
// spend: { spender }
// transaction: { exists, height, output }

populate_block::populate_block(threadpool& priority_pool,
    const fast_chain& chain)
  : populate_base(priority_pool, chain)
{
}

void populate_block::populate(branch::const_ptr branch,
    result_handler&& handler) const
{
    const auto block = branch->top();
    BITCOIN_ASSERT(block);

    const auto state = block->validation.state;
    BITCOIN_ASSERT(state);

    // Return if this blocks is under a checkpoint, block state not requried.
    if (state->is_under_checkpoint())
    {
        handler(error::success);
        return;
    }

    populate_coinbase(block);

    //*************************************************************************
    // CONSENSUS: Satoshi implemented this change in Nov 2015. This was a hard 
    // branch that will produce catostrophic results in the case of a hash
    // collision. Unspent duplicate check has cost but should not be skipped.
    //*************************************************************************
    if (!state->is_enabled(machine::rule_fork::allow_collisions))
    {
        //*********************************************************************
        // CONSENSUS: The tx duplicate check must apply to coinbase txs.
        //*********************************************************************
        // TODO: consider paralellize by transaction.
        for (auto& tx: block->transactions())
        {
            populate_base::populate_duplicate(branch->height(), tx);
            populate_duplicate(branch, tx);
        }
    }

    const auto non_coinbase_inputs = block->total_inputs(false);

    // Return if there are no non-coinbase inputs to validate.
    if (non_coinbase_inputs == 0)
    {
        handler(error::success);
        return;
    }

    const auto threads = dispatch_.size();
    const auto buckets = std::min(threads, non_coinbase_inputs);
    const auto join_handler = synchronize(std::move(handler), buckets, NAME);
    BITCOIN_ASSERT(threads != 0);

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        dispatch_.concurrent(&populate_block::populate_inputs,
            this, branch, bucket, buckets, join_handler);
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

void populate_block::populate_duplicate(branch::const_ptr branch,
    const chain::transaction& tx) const
{
    if (!tx.validation.duplicate)
        branch->populate_tx(tx);
}

void populate_block::populate_inputs(branch::const_ptr branch,
    size_t bucket, size_t buckets, result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    const auto block = branch->top();
    const auto branch_height = branch->height();
    const auto& txs = block->transactions();
    size_t position = 0;

    // Must skip coinbase here as it is already accounted for.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        const auto& inputs = tx->inputs();

        for (size_t input_index = 0; input_index < inputs.size();
            ++input_index, ++position)
        {
            // TODO: eliminate the wasteful iterations by using smart step.
            if (position % buckets != bucket)
                continue;

            const auto& input = inputs[input_index];
            const auto& output = input.previous_output();
            populate_base::populate_prevout(branch_height, output);
            populate_prevout(branch, output);
        }
    }

    handler(error::success);
}

void populate_block::populate_prevout(branch::const_ptr branch,
    const output_point& outpoint) const
{
    if (!outpoint.validation.spent)
        branch->populate_spent(outpoint);

    // Populate the previous output even if it is spent.
    if (!outpoint.validation.cache.is_valid())
        branch->populate_prevout(outpoint);
}

} // namespace blockchain
} // namespace libbitcoin
