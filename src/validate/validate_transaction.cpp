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
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_input.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;
using namespace std::placeholders;

#define NAME "validate_transaction"

validate_transaction::validate_transaction(dispatcher& dispatch,
    const fast_chain& chain, const settings& settings)
  : stopped_(true),
    retarget_(settings.retarget),
    use_libconsensus_(settings.use_libconsensus),
    dispatch_(dispatch),
    transaction_populator_(dispatch, chain)
{
}

// Properties.
//-----------------------------------------------------------------------------

bool validate_transaction::stopped() const
{
    return stopped_;
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

    // Skip validation when valid tx is already stored with same forks.
    if (tx->metadata.pooled)
    {
        handler(error::success);
        return;
    }

    // Run contextual tx checks.
    handler(tx->accept());
}

// Connect sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, block state and perform script metadata.

void validate_transaction::connect(transaction_const_ptr tx,
    result_handler handler) const
{
    const auto total_inputs = tx->inputs().size();
    const auto buckets = std::min(dispatch_.size(), total_inputs);
    const auto join_handler = synchronize(handler, buckets, NAME "_validate");
    BITCOIN_ASSERT_MSG(buckets != 0, "transaction check must require inputs");

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
    BITCOIN_ASSERT(tx->metadata.state);

    code ec(error::success);
    const auto forks = tx->metadata.state->enabled_forks();
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

        if (!prevout.metadata.cache.is_valid())
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
