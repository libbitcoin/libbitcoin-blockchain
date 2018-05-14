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
#include <bitcoin/blockchain/populate/populate_transaction.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace std::placeholders;

#define NAME "populate_transaction"

populate_transaction::populate_transaction(dispatcher& dispatch,
    const fast_chain& chain)
  : populate_base(dispatch, chain)
{
}

void populate_transaction::populate(transaction_const_ptr tx,
    result_handler&& handler) const
{
    auto& metadata = tx->metadata;

    // Get the chain state of the next block (tx pool).
    metadata.state = fast_chain_.next_confirmed_state();
    BITCOIN_ASSERT(metadata.state);

    fast_chain_.populate_pool_transaction(*tx, metadata.state->enabled_forks());

    // The tx is already confirmed (nothing to do).
    if (metadata.confirmed)
    {
        handler(error::duplicate_transaction);
        return;
    }

    // The tx is already verified for the pool (nothing to do).
    if (metadata.verified)
    {
        handler(error::duplicate_transaction);
        return;
    }

    const auto total_inputs = tx->inputs().size();
    const auto buckets = std::min(dispatch_.size(), total_inputs);
    const auto join_handler = synchronize(std::move(handler), buckets, NAME);
    BITCOIN_ASSERT_MSG(buckets != 0, "transaction check must require inputs");

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        dispatch_.concurrent(&populate_transaction::populate_inputs,
            this, tx, bucket, buckets, join_handler);
}

void populate_transaction::populate_inputs(transaction_const_ptr tx,
    size_t bucket, size_t buckets, result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    const auto& inputs = tx->inputs();

    for (auto input_index = bucket; input_index < inputs.size();
        input_index = ceiling_add(input_index, buckets))
    {
        const auto& input = inputs[input_index];
        const auto& prevout = input.previous_output();
        fast_chain_.populate_output(prevout, max_size_t, false);
    }

    handler(error::success);
}

} // namespace blockchain
} // namespace libbitcoin
