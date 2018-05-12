/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;

#define NAME "populate_block"

populate_block::populate_block(dispatcher& dispatch, const fast_chain& chain)
  : populate_base(dispatch, chain)
{
}

// Returns store code only.
void populate_block::populate(block_const_ptr block,
    result_handler&& handler) const
{
    // The block class has no population method, so set timer externally.
    block->metadata.start_populate = asio::steady_clock::now();
    auto& metadata = block->header().metadata;

    // If previously failed skip validation.
    if (metadata.error)
    {
        handler(error::success);
        return;
    }

    // The next candidate must be that which follows the last valid candidate.
    metadata.state = fast_chain_.promote_state(block->header(),
        fast_chain_.top_valid_candidate_state());

    if (!metadata.state)
    {
        handler(error::operation_failed);
        return;
    }

    // TODO: if not validating then skip tx/block population.
    // TODO: if neither validating nor indexing skip prevout population.
    // TODO: populate header metadata on block read and then skip here.
    // TODO: populate tx metadata on tx/block read and then skip here.

    // Populate header metadata if it hasn't been populated.
    // The exists value defaults false but will be true if metadata populated.
    if (!metadata.exists)
        fast_chain_.populate_header(block->header());

    // Above this confirmed are not confirmed in the candidate chain.
    const auto fork_height = fast_chain_.fork_point().height();

    // Populate the coinbase as a special case tx.
    populate_coinbase(block, fork_height);

    const auto non_coinbase_inputs = block->total_non_coinbase_inputs();

    // Return if there are no non-coinbase inputs to validate.
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
            this, block, fork_height, bucket, buckets, join_handler);
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

    // A coinbase input cannot be a double spend since it originates coin.
    prevout.spent = false;

    // A coinbase prevout is always considered confirmed just for consistency.
    prevout.candidate = false;
    prevout.confirmed = true;

    // A coinbase does not spend a previous output so these are unused/default.
    prevout.coinbase = false;
    prevout.height = 0;
    prevout.median_time_past = 0;
    prevout.cache = chain::output{};

    const auto forks = block->header().metadata.state->enabled_forks();
    fast_chain_.populate_block_transaction(tx, forks, fork_height);
}

void populate_block::populate_transactions(block_const_ptr block,
    size_t fork_height, size_t bucket, size_t buckets,
    result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    const auto& txs = block->transactions();
    const auto state = block->header().metadata.state;
    const auto forks = state->enabled_forks();
    size_t input_position = 0;

    // Must skip coinbase here as it is already accounted for.
    for (auto position = bucket == 0 ? buckets : bucket; position < txs.size();
        position = ceiling_add(position, buckets))
    {
        const auto& tx = txs[position];
        fast_chain_.populate_block_transaction(tx, forks, fork_height);
    }

    // Must skip coinbase here as it is already accounted for.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        const auto& inputs = tx->inputs();

        for (size_t input_index = 0; input_index < inputs.size();
            ++input_index, ++input_position)
        {
            if (input_position % buckets != bucket)
                continue;

            const auto& prevout = inputs[input_index].previous_output();
            fast_chain_.populate_output(prevout, fork_height, true);
        }
    }

    handler(error::success);
}

} // namespace blockchain
} // namespace libbitcoin
