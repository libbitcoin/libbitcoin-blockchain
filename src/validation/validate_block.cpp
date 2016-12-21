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
#include <bitcoin/blockchain/validation/validate_block.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/populate_block.hpp>
#include <bitcoin/blockchain/validation/validate_input.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;
using namespace std::placeholders;

#define NAME "validate_block"

// Database access is limited to: populator:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

validate_block::validate_block(threadpool& priority_pool,
    const fast_chain& chain, const settings& settings)
  : stopped_(false),
    use_libconsensus_(settings.use_libconsensus),
    priority_dispatch_(priority_pool, NAME "_dispatch"),
    populator_(priority_pool, chain, settings)
{
}

// Stop sequence.
//-----------------------------------------------------------------------------

// There is no need to maange the priority thread pool as all threads must be
// joined to unblock the calling thread, so the stopped flag is sufficient.
void validate_block::stop()
{
    populator_.stop();
    stopped_.store(true);
}

bool validate_block::stopped() const
{
    return stopped_.load();
}

// Check.
//-----------------------------------------------------------------------------
// These checks are context free.

code validate_block::check(block_const_ptr block) const
{
    // Run context free checks, sets time internally.
    return block->check();
}

// Accept sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, and block state if not under checkpoint.

void validate_block::accept(fork::const_ptr fork, result_handler handler) const
{
    const auto block = fork->top();
    BITCOIN_ASSERT(block);

    // The block has no population timer, so set externally.
    block->validation.start_populate = asio::steady_clock::now();

    result_handler complete_handler =
        std::bind(&validate_block::handle_populated,
            this, _1, block, handler);

    // Populate chain state and block state as required.
    populator_.populate_block_state(fork, std::move(complete_handler));
}

void validate_block::handle_populated(const code& ec, block_const_ptr block,
    result_handler handler) const
{
    BITCOIN_ASSERT(block->validation.state);

    if (ec)
    {
        handler(ec);
        return;
    }

    // Run contextual block non-tx checks (sets start time).
    const auto error_code = block->accept(false);

    if (error_code)
    {
        handler(error_code);
        return;
    }

    const auto state = block->validation.state;
    const auto sigops = std::make_shared<atomic_counter>(0);

    result_handler complete_handler =
        std::bind(&validate_block::handle_accepted,
            this, _1, block, sigops, handler);

    if (state->is_under_checkpoint())
    {
        complete_handler(error::success);
        return;
    }

    const auto count = block->transactions().size();
    auto bip16 = state->is_enabled(rule_fork::bip16_rule);
    const auto threads = std::min(priority_dispatch_.size(), count);
    const auto join_handler = synchronize(std::move(complete_handler), threads,
        NAME "_accept");

    for (size_t bucket = 0; bucket < threads; ++bucket)
        priority_dispatch_.concurrent(&validate_block::accept_transactions,
            this, block, bucket, sigops, bip16, join_handler);
}

void validate_block::accept_transactions(block_const_ptr block, size_t bucket,
    atomic_counter_ptr sigops, bool bip16, result_handler handler) const
{
    code ec(error::success);
    const auto buckets = priority_dispatch_.size();
    const auto& state = *block->validation.state;
    const auto& txs = block->transactions();
    const auto count = txs.size();

    // Run contextual tx non-script checks (not in tx order).
    for (auto tx = bucket; tx < count && !ec; tx = ceiling_add(tx, buckets))
    {
        const auto& transaction = txs[tx];
        ec = transaction.accept(state, false);
        *sigops += transaction.signature_operations(bip16);
    }

    handler(ec);
}

void validate_block::handle_accepted(const code& ec, block_const_ptr block,
    atomic_counter_ptr sigops, result_handler handler) const
{
    if (ec)
    {
        handler(ec);
        return;
    }

    const auto exceeded = *sigops > max_block_sigops;
    handler(exceeded ? error::block_embedded_sigop_limit : error::success);
}

// Connect sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, block state and perform script validation.

void validate_block::connect(fork::const_ptr fork,
    result_handler handler) const
{
    const auto block = fork->top();
    BITCOIN_ASSERT(block && block->validation.state);

    // We are reimplemeting connect, so set must timer externally.
    block->validation.start_connect = asio::steady_clock::now();

    if (block->validation.state->is_under_checkpoint())
    {
        handler(error::success);
        return;
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
    const auto join_handler = synchronize(handler, buckets,
        NAME "_validate");

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        priority_dispatch_.concurrent(&validate_block::connect_inputs,
            this, block, bucket, buckets, join_handler);
}

// TODO: move to validate_input.hpp/cpp (static methods only).
void validate_block::connect_inputs(block_const_ptr block, size_t bucket,
    size_t buckets, result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);
    code ec(error::success);
    const auto forks = block->validation.state->enabled_forks();
    const auto& txs = block->transactions();
    size_t position = 0;

    // Must skip coinbase here as it is already accounted for.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        size_t input_index;
        const auto& inputs = tx->inputs();

        // TODO: eliminate the wasteful iterations by using smart step.
        for (input_index = 0; input_index < inputs.size();
            ++input_index, ++position)
        {
            if (position % buckets != bucket)
                continue;

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

        if (ec)
        {
            const auto height = block->validation.state->height();
            dump(ec, *tx, input_index, forks, height, use_libconsensus_);
            break;
        }
    }

    handler(ec);
}

// Utility.
//-----------------------------------------------------------------------------

void validate_block::dump(const code& ec, const transaction& tx,
    uint32_t input_index, uint32_t forks, size_t height, bool use_libconsensus)
{
    const auto& prevout = tx.inputs()[input_index].previous_output();
    const auto script = prevout.validation.cache.script().to_data(false);
    const auto hash = encode_hash(prevout.hash());
    const auto tx_hash = encode_hash(tx.hash());

    LOG_DEBUG(LOG_BLOCKCHAIN)
        << "Verify failed [" << height << "] : " << ec.message() << std::endl
        << " libconsensus : " << use_libconsensus << std::endl
        << " forks        : " << forks << std::endl
        << " outpoint     : " << hash << ":" << prevout.index() << std::endl
        << " script       : " << encode_base16(script) << std::endl
        << " inpoint      : " << tx_hash << ":" << input_index << std::endl
        << " transaction  : " << encode_base16(tx.to_data());
}

} // namespace blockchain
} // namespace libbitcoin
