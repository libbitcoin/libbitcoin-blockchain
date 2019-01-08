/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/populate/populate_block.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;
using namespace bc::system::chain;
using namespace bc::system::machine;

#define NAME "populate_block"

populate_block::populate_block(dispatcher& dispatch, const fast_chain& chain,
    bool catalog)
  : populate_base(dispatch, chain),
    catalog_(catalog)
{
}

// Returns store code only.
void populate_block::populate(block_const_ptr block,
    result_handler&& handler) const
{
    // This candidate must be that which follows the top valid candidate.
    auto& metadata = block->header().metadata;
    const auto top_valid = fast_chain_.top_valid_candidate_state();
    metadata.state = fast_chain_.promote_state(block->header(), top_valid);

    if (!metadata.state)
    {
        handler(error::operation_failed);
        return;
    }

    // Above this confirmed are not confirmed in the candidate chain.
    const auto fork_height = fast_chain_.fork_point().height();

    // Contextual validation bypassed if already validated or under checkpoint.
    if (metadata.validated || metadata.state->is_under_checkpoint())
    {
        // Skip prevout population if not cataloging payments.
        if (catalog_)
            populate_non_coinbase(block, fork_height, false, handler);
        else
            handler(error::success);

        return;
    }

    populate_coinbase(block, fork_height);
    populate_non_coinbase(block, fork_height, true, handler);
}

void populate_block::populate_non_coinbase(block_const_ptr block,
    size_t fork_height, bool populate_txs, result_handler handler) const
{
    const auto non_coinbase_inputs = block->total_non_coinbase_inputs();

    if (non_coinbase_inputs == 0)
    {
        handler(error::success);
        return;
    }

    const auto buckets = std::min(dispatch_.size(), non_coinbase_inputs);
    const auto join_handler = synchronize(std::move(handler), buckets, NAME);
    BITCOIN_ASSERT(buckets != 0);

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        dispatch_.concurrent(&populate_block::populate_transactions,
            this, block, fork_height, bucket, buckets, populate_txs,
                join_handler);
}

// Initialize the coinbase input for subsequent metadata.
void populate_block::populate_coinbase(block_const_ptr block,
    size_t fork_height) const
{
    const auto& txs = block->transactions();
    BITCOIN_ASSERT(!txs.empty());

    const auto& tx = txs.front();
    BITCOIN_ASSERT(tx.is_coinbase());

    // A coinbase tx guarantees exactly one input.
    auto& prevout = tx.inputs().front().previous_output().metadata;

    // A coinbase prevout is always considered confirmed just for consistency.
    prevout.candidate = false;
    prevout.confirmed = true;

    // A coinbase input has no output to have been spent.
    prevout.candidate_spent = false;
    prevout.confirmed_spent = false;

    // A coinbase does not spend a previous output so these are unused/default.
    prevout.height = 0;
    prevout.coinbase = false;
    prevout.median_time_past = 0;
    prevout.cache = chain::output{};

    const auto forks = block->header().metadata.state->enabled_forks();
    fast_chain_.populate_block_transaction(tx, forks, fork_height);
}

void populate_block::populate_transactions(block_const_ptr block,
    size_t fork_height, size_t bucket, size_t buckets, bool populate_txs,
    result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    const auto& txs = block->transactions();
    const auto state = block->header().metadata.state;
    const auto forks = state->enabled_forks();
    size_t input_position = 0;

    if (populate_txs)
    {
        // Must skip coinbase here as it is already accounted for.
        for (auto position = (bucket == 0 ? buckets : bucket);
            position < txs.size(); position = ceiling_add(position, buckets))
        {
            const auto& tx = txs[position];
            fast_chain_.populate_block_transaction(tx, forks, fork_height);
        }
    }

    // Must skip coinbase here as it is already accounted for.
    for (auto tx = std::next(txs.begin()); tx != txs.end(); ++tx)
    {
        const auto& inputs = tx->inputs();

        for (size_t input_index = 0; input_index < inputs.size();
            ++input_index, ++input_position)
        {
            if (input_position % buckets != bucket)
                continue;

            const auto& prevout = inputs[input_index].previous_output();

            // Don't fail here if output is missing, populate all.
            /*bool*/ fast_chain_.populate_block_output(prevout, fork_height);
        }
    }

    handler(error::success);
}

} // namespace blockchain
} // namespace libbitcoin
