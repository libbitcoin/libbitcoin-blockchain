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
#include <bitcoin/blockchain/populate/populate_transaction.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace std::placeholders;

#define NAME "populate_transaction"

// Database access is limited to:
// spend: { spender }
// transaction: { exists, height, output }

populate_transaction::populate_transaction(threadpool& pool,
    const fast_chain& chain)
  : populate_base(pool, chain)
{
}

void populate_transaction::populate(transaction_const_ptr tx,
    result_handler&& handler) const
{
    const auto state = tx->validation.state;
    BITCOIN_ASSERT(state);

    // Chain state is for the next block, so always > 0.
    BITCOIN_ASSERT(tx->validation.state->height() > 0);
    const auto chain_height = tx->validation.state->height() - 1u;

    //*************************************************************************
    // CONSENSUS: Satoshi implemented this block validation change in Nov 2015.
    // We must follow the same rules in the tx pool to support block tempaltes.
    //*************************************************************************
    if (!state->is_enabled(machine::rule_fork::allowed_duplicates))
        populate_duplicate(chain_height, *tx);

    const auto total_inputs = tx->inputs().size();

    // Return if there are no inputs to validate (will fail later).
    if (total_inputs == 0)
    {
        handler(error::success);
        return;
    }

    const auto threads = dispatch_.size();
    const auto buckets = std::min(threads, total_inputs);
    const auto join_handler = synchronize(std::move(handler), buckets, NAME);

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        dispatch_.concurrent(&populate_transaction::populate_inputs,
            this, *tx, chain_height, bucket, buckets, join_handler);
}

void populate_transaction::populate_inputs(const chain::transaction& tx,
    size_t chain_height, size_t bucket, size_t buckets,
    result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    const auto& inputs = tx.inputs();

    for (auto input_index = bucket; input_index < inputs.size();
        input_index = ceiling_add(input_index, bucket))
    {
        const auto& input = inputs[input_index];
        const auto& output = input.previous_output();
        populate_prevout(chain_height, output);
    }

    handler(error::success);
}

} // namespace blockchain
} // namespace libbitcoin
