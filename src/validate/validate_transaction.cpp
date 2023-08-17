/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_input.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;
using namespace std::placeholders;

#define NAME "validate_transaction"

// Database access is limited to: populator:
// spend: { spender }
// transaction: { exists, height, output }

validate_transaction::validate_transaction(dispatcher& dispatch,
    const fast_chain& chain, const settings& settings)
  : stopped_(true),
    retarget_(settings.retarget),
    use_libconsensus_(settings.use_libconsensus),
    dispatch_(dispatch),
    transaction_populator_(dispatch, chain),
    fast_chain_(chain)
{
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

void validate_transaction::start()
{
    stopped_ = false;
}

void validate_transaction::stop()
{
    stopped_ = true;
}

// Check.
//-----------------------------------------------------------------------------
// These checks are context free.

void validate_transaction::check(transaction_const_ptr tx,
    result_handler handler) const
{
    // Run context free checks.
    handler(tx->check(true, retarget_));
}

// Accept sequence.
//-----------------------------------------------------------------------------
// These checks require chain and tx state (net height and enabled forks).

void validate_transaction::accept(transaction_const_ptr tx,
    result_handler handler) const
{
    // Populate chain state of the next block (tx pool).
    tx->validation.state = fast_chain_.chain_state();

    if (!tx->validation.state)
    {
        handler(error::operation_failed);
        return;
    }

    transaction_populator_.populate(tx,
        std::bind(&validate_transaction::handle_populated,
            this, _1, tx, handler));
}

void validate_transaction::handle_populated(const code& ec,
    transaction_const_ptr tx, result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        handler(ec);
        return;
    }

    BITCOIN_ASSERT(tx->validation.state);

    // Run contextual tx checks.
    handler(tx->accept());
}

// Connect sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, block state and perform script validation.

void validate_transaction::connect(transaction_const_ptr tx,
    result_handler handler) const
{
    BITCOIN_ASSERT(tx->validation.state);
    const auto total_inputs = tx->inputs().size();

    // Return if there are no inputs to validate (will fail later).
    if (total_inputs == 0)
    {
        handler(error::success);
        return;
    }

    const auto buckets = std::min(dispatch_.size(), total_inputs);
    const auto join_handler = synchronize(handler, buckets, NAME "_validate");
    BITCOIN_ASSERT(buckets != 0);

    // If the priority threadpool is shut down when this is called the handler
    // will never be invoked, resulting in a threadpool.join indefinite hang.
    for (size_t bucket = 0; bucket < buckets; ++bucket)
        dispatch_.concurrent(&validate_transaction::connect_inputs,
            this, tx, bucket, buckets, join_handler);
}

void validate_transaction::connect_inputs(transaction_const_ptr tx,
    size_t bucket, size_t buckets, result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    code ec(error::success);
    const auto forks = tx->validation.state->enabled_forks();
    const auto& inputs = tx->inputs();

    for (auto input_index = bucket; input_index < inputs.size();
        input_index = ceiling_add(input_index, buckets))
    {
        if (stopped())
        {
            ec = error::service_stopped;
            break;
        }

        const auto& prevout = inputs[input_index].previous_output();

        if (!prevout.validation.cache.is_valid())
        {
            ec = error::missing_previous_output;
            break;
        }

        if ((ec = validate_input::verify_script(*tx, input_index, forks,
            use_libconsensus_)))
        {
            break;
        }
    }

    handler(ec);
}

} // namespace blockchain
} // namespace libbitcoin
