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
#include <bitcoin/blockchain/validate/validate_block.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_input.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;
using namespace std::placeholders;

#define NAME "validate_block"

validate_block::validate_block(dispatcher& dispatch, const fast_chain& chain,
    const settings& settings)
  : stopped_(true),
    retarget_(settings.retarget),
    use_libconsensus_(settings.use_libconsensus),
    priority_dispatch_(dispatch),
    block_populator_(dispatch, chain)
{
}

// Properties.
//-----------------------------------------------------------------------------

bool validate_block::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

void validate_block::start()
{
    stopped_ = false;
}

void validate_block::stop()
{
    stopped_ = true;
}

// Check.
//-----------------------------------------------------------------------------
// These checks are context free.

void validate_block::check(block_const_ptr block, result_handler handler) const
{
    // Guard aganst zero threads dispatch.
    if (block->transactions().empty())
    {
        handler(error::empty_block);
        return;
    }

////    result_handler complete_handler =
////        std::bind(&validate_block::handle_checked,
////            this, _1, block, handler);
////
////    // TODO: make configurable for each parallel segment.
////    // This one is more efficient with one thread than parallel.
////    const auto threads = std::min(size_t(1), priority_dispatch_.size());
////
////    const auto count = block->transactions().size();
////    const auto buckets = std::min(threads, count);
////    BITCOIN_ASSERT(buckets != 0);
////
////    const auto join_handler = synchronize(std::move(complete_handler), buckets,
////        NAME "_check");
////
////    for (size_t bucket = 0; bucket < buckets; ++bucket)
////        priority_dispatch_.concurrent(&validate_block::check_block,
////            this, block, bucket, buckets, join_handler);
////}
////
////void validate_block::check_block(block_const_ptr block, size_t bucket,
////    size_t buckets, result_handler handler) const
////{
////    if (stopped())
////    {
////        handler(error::service_stopped);
////        return;
////    }
////
////    const auto& txs = block->transactions();
////
////    // Generate each tx hash (stored in tx cache).
////    for (auto tx = bucket; tx < txs.size(); tx = ceiling_add(tx, buckets))
////        txs[tx].hash();
////
////    handler(error::success);
////}
////
////void validate_block::handle_checked(const code& ec, block_const_ptr block,
////    result_handler handler) const
////{
////    if (ec)
////    {
////        handler(ec);
////        return;
////    }

    // Run context free checks, sets time internally.
    handler(block->check(retarget_));
}

// Accept sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, and block state if not under checkpoint.

void validate_block::accept(block_const_ptr block,
    result_handler handler) const
{
    // Populate block state for the next block.
    block_populator_.populate(block,
        std::bind(&validate_block::handle_populated,
            this, _1, block, handler));
}

void validate_block::handle_populated(const code& ec, block_const_ptr block,
    result_handler handler) const
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

    // Run contextual block non-tx checks (sets start time).
    const auto error_code = block->accept(false);

    if (error_code)
    {
        handler(error_code);
        return;
    }

    const auto sigops = std::make_shared<atomic_counter>(0);
    const auto state = block->header().metadata.state;
    BITCOIN_ASSERT(state);

    const auto bip141 = state->is_enabled(rule_fork::bip141_rule);

    result_handler complete_handler =
        std::bind(&validate_block::handle_accepted,
            this, _1, block, sigops, bip141, handler);

    if (state->is_under_checkpoint())
    {
        complete_handler(error::success);
        return;
    }

    const auto count = block->transactions().size();
    const auto bip16 = state->is_enabled(rule_fork::bip16_rule);
    const auto buckets = std::min(priority_dispatch_.size(), count);
    BITCOIN_ASSERT_MSG(buckets != 0, "block check must require transactions");

    const auto join_handler = synchronize(std::move(complete_handler), buckets,
        NAME "_accept");

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        priority_dispatch_.concurrent(&validate_block::accept_transactions,
            this, block, bucket, buckets, sigops, bip16, bip141, join_handler);
}

void validate_block::accept_transactions(block_const_ptr block, size_t bucket,
    size_t buckets, atomic_counter_ptr sigops, bool bip16, bool bip141,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    code ec(error::success);
    const auto& state = *block->header().metadata.state;
    const auto& txs = block->transactions();
    const auto count = txs.size();

    // Run contextual tx non-script checks (not in tx order).
    for (auto tx = bucket; tx < count && !ec; tx = ceiling_add(tx, buckets))
    {
        const auto& transaction = txs[tx];
        ec = transaction.accept(state, false);
        *sigops += transaction.signature_operations(bip16, bip141);
    }

    handler(ec);
}

void validate_block::handle_accepted(const code& ec, block_const_ptr ,
    atomic_counter_ptr sigops, bool bip141, result_handler handler) const
{
    if (ec)
    {
        handler(ec);
        return;
    }

    const auto max_sigops = bip141 ? max_fast_sigops : max_block_sigops;
    const auto exceeded = *sigops > max_sigops;
    handler(exceeded ? error::block_embedded_sigop_limit : error::success);
}

// Connect sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, block state and perform script metadata.

void validate_block::connect(block_const_ptr block,
    result_handler handler) const
{
    // We are reimplementing connect, so must set timer externally.
    block->metadata.start_connect = asio::steady_clock::now();

    const auto state = block->header().metadata.state;
    BITCOIN_ASSERT(state);

    if (state->is_under_checkpoint())
    {
        handler(error::success);
        return;
    }

    const auto non_coinbase_inputs = block->total_non_coinbase_inputs();

    // Return if there are no non-coinbase inputs to validate.
    if (non_coinbase_inputs == 0)
    {
        handler(error::success);
        return;
    }

    // Reset statistics for each block (treat coinbase as cached).
    hits_ = 0;
    queries_ = 0;

    result_handler complete_handler =
        std::bind(&validate_block::handle_connected,
            this, _1, block, handler);

    const auto threads = priority_dispatch_.size();
    const auto buckets = std::min(threads, non_coinbase_inputs);
    BITCOIN_ASSERT(buckets != 0);

    const auto join_handler = synchronize(std::move(complete_handler), buckets,
        NAME "_validate");

    for (size_t bucket = 0; bucket < buckets; ++bucket)
        priority_dispatch_.concurrent(&validate_block::connect_inputs,
            this, block, bucket, buckets, join_handler);
}

void validate_block::connect_inputs(block_const_ptr block, size_t bucket,
    size_t buckets, result_handler handler) const
{
    BITCOIN_ASSERT(bucket < buckets);

    code ec(error::success);
    const auto state = block->header().metadata.state;
    const auto forks = state->enabled_forks();
    const auto& txs = block->transactions();
    size_t position = 0;

    // Must skip coinbase here as it is already accounted for.
    for (auto tx = txs.begin() + 1; tx != txs.end(); ++tx)
    {
        ++queries_;

        // The tx is pooled with current fork state so outputs are validated.
        if (tx->metadata.pooled)
        {
            ++hits_;
            continue;
        }

        size_t input_index;
        const auto& inputs = tx->inputs();

        for (input_index = 0; input_index < inputs.size();
            ++input_index, ++position)
        {
            if (position % buckets != bucket)
                continue;

            if (stopped())
            {
                handler(error::service_stopped);
                return;
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

        if (ec)
        {
            const auto height = state->height();
            dump(ec, *tx, input_index, forks, height, use_libconsensus_);
            break;
        }
    }

    handler(ec);
}

// The tx pool cache hit rate.
float validate_block::hit_rate() const
{
    // These values could overflow or divide by zero, but that's okay.
    return queries_ == 0 ? 0.0f : (hits_ * 1.0f / queries_);
}

void validate_block::handle_connected(const code& ec, block_const_ptr block,
    result_handler handler) const
{
    block->metadata.cache_efficiency = hit_rate();
    handler(ec);
}

// Utility.
//-----------------------------------------------------------------------------

void validate_block::dump(const code& ec, const transaction& tx,
    uint32_t input_index, uint32_t forks, size_t height, bool use_libconsensus)
{
    const auto& prevout = tx.inputs()[input_index].previous_output();
    const auto script = prevout.metadata.cache.script().to_data(false);
    const auto hash = encode_hash(prevout.hash());
    const auto tx_hash = encode_hash(tx.hash());

    LOG_DEBUG(LOG_BLOCKCHAIN)
        << "Verify failed [" << height << "] : " << ec.message() << std::endl
        << " libconsensus : " << use_libconsensus << std::endl
        << " forks        : " << forks << std::endl
        << " outpoint     : " << hash << ":" << prevout.index() << std::endl
        << " script       : " << encode_base16(script) << std::endl
        << " value        : " << prevout.metadata.cache.value() << std::endl
        << " inpoint      : " << tx_hash << ":" << input_index << std::endl
        << " transaction  : " << encode_base16(tx.to_data(true, true));
}

} // namespace blockchain
} // namespace libbitcoin
