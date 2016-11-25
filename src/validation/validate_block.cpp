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
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <thread>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/populate_block.hpp>
#include <bitcoin/blockchain/validation/validate_input.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace std::placeholders;

#define NAME "validate_block"

// Constant for log report calculations.
static constexpr size_t micro_per_milliseconds = 1000;

// Database access is limited to: populator:
// spend: { spender }
// block: { bits, version, timestamp }
// transaction: { exists, height, output }

static inline size_t cores(const settings& settings)
{
    const auto configured = settings.threads;
    const auto hardware = std::max(std::thread::hardware_concurrency(), 1u);
    return configured == 0 ? hardware : std::min(configured, hardware);
}

static inline thread_priority priority(const settings& settings)
{
    return settings.priority ? thread_priority::high : thread_priority::normal;
}

validate_block::validate_block(threadpool& pool, const fast_chain& chain,
    const settings& settings)
  : stopped_(false),
    use_libconsensus_(settings.use_libconsensus),
    priority_pool_(cores(settings), priority(settings)),
    dispatch_(priority_pool_, NAME "_dispatch"),
    populator_(priority_pool_, chain, settings)
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
    const auto start_time = asio::steady_clock::now();

    // Run context free checks.
    const auto ec = block->check();

    report(block, start_time, ec ? "UNCHECKED" : "checked  ");
    return ec;

    // The above includes no height and the cost is tiny, so not reporting.
    return block->check();
}

// Accept sequence.
//-----------------------------------------------------------------------------

// These checks require chain state, and block state if not under checkpoint.
void validate_block::accept(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    BITCOIN_ASSERT(index < fork->size());

    const auto block = fork->block_at(index);
    const auto height = fork->height_at(index);
    const auto checkpointed = block->validation.state->is_under_checkpoint();

    const result_handler populated_handler =
        std::bind(&validate_block::handle_accept,
            this, _1, block, height, asio::steady_clock::now(), handler);

    // Skip chain state population if already populated.
    if (!block->validation.state)
        populator_.populate_chain_state(fork, index);

    // Skip block state population if not needed or already populated.
    if (checkpointed || block->validation.sets)
        populated_handler(error::success);
    else
        populator_.populate_block_state(fork, index, populated_handler);
}

void validate_block::handle_accept(const code& ec, block_const_ptr block,
    size_t height, const asio::time_point& start_time,
    result_handler handler) const
{
    if (ec)
    {
        handler(ec);
        return;
    }

    report(block, start_time, ec ? "UNPOPULATED" : "populated");
    const auto accept_start_time = asio::steady_clock::now();

    // Run contextual non-script checks.
    // TODO: parallelize block accept by transaction.
    const auto error_code = block->accept();

    report(block, accept_start_time, error_code ? "REJECTED  " : "accepted ");
    handler(error_code);
}

// Connect sequence.
//-----------------------------------------------------------------------------
// These checks require chain state, block state and perform script validation.

void validate_block::connect(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    BITCOIN_ASSERT(index < fork->size());

    const auto block = fork->block_at(index);
    const auto height = fork->height_at(index);

    const result_handler complete_handler =
        std::bind(&validate_block::handle_connected,
            this, _1, block, height, asio::steady_clock::now(), handler);

    if (block->validation.state->is_under_checkpoint())
    {
        complete_handler(error::success);
        return;
    }

    const auto sets = block->validation.sets;
    const auto state = block->validation.state;

    if (!sets || !state)
    {
        handler(error::operation_failed);
        return;
    }

    // Sets will be empty if there is only a coinbase tx.
    if (sets->empty())
    {
        handler(error::success);
        return;
    }

    const result_handler join_handler = synchronize(complete_handler,
        sets->size(), NAME "_validate");

    for (size_t set = 0; set < sets->size(); ++set)
        dispatch_.concurrent(&validate_block::connect_inputs,
            this, sets, set, state->enabled_forks(), join_handler);
}

// TODO: move to validate_input.hpp/cpp (static methods only).
void validate_block::connect_inputs(transaction::sets_const_ptr input_sets,
    size_t sets_index, uint32_t flags, result_handler handler) const
{
    BITCOIN_ASSERT(!input_sets->empty() && sets_index < input_sets->size());

    code ec(error::success);
    const auto& sets = (*input_sets)[sets_index];

    for (auto& set: sets)
    {
        BITCOIN_ASSERT(!set.tx.is_coinbase());
        BITCOIN_ASSERT(set.input_index < set.tx.inputs().size());
        const auto& input = set.tx.inputs()[set.input_index];

        if (stopped())
        {
            ec = error::service_stopped;
            break;
        }

        if (!input.previous_output().validation.cache.is_valid())
        {
            ec = error::missing_input;
            break;
        }

        if ((ec = validate_input::verify_script(set.tx, set.input_index, flags,
            use_libconsensus_)))
            break;
    }

    handler(ec);
}

void validate_block::handle_connected(const code& ec, block_const_ptr block,
    size_t height, const asio::time_point& start_time,
    result_handler handler) const
{
    if (ec)
    {
        handler(ec);
        return;
    }

    report(block, start_time, ec ? "INVALIDATED" : "validated");
    handler(ec);
}

// Utility.
//-----------------------------------------------------------------------------

inline bool enabled(size_t height)
{
    // Vary the reporting performance reporting interval by height.
    const auto modulus =
        (height < 100000 ? 1000 :
        (height < 200000 ? 100 :
        (height < 300000 ? 10 : 1)));

    return height % modulus == 0;
}

void validate_block::report(block_const_ptr block,
    const asio::time_point& start_time, const std::string& token)
{
    BITCOIN_ASSERT(block->validation.state);
    const auto height = block->validation.state->height();

    if (enabled(height))
    {
        const auto delta = asio::steady_clock::now() - start_time;
        const auto elapsed = std::chrono::duration_cast<asio::microseconds>(delta);
        const auto micro_per_block = static_cast<float>(elapsed.count());
        const auto nonzero_inputs = std::max(block->total_inputs(), size_t(1));
        const auto micro_per_input = micro_per_block / nonzero_inputs;
        const auto milli_per_block = micro_per_block / micro_per_milliseconds;
        const auto transactions = block->transactions().size();

        LOG_INFO(LOG_BLOCKCHAIN)
            << "Block [" << height << "] " << token << " (" << transactions
            << ") txs in (" << milli_per_block << ") ms or (" << micro_per_input
            << ") μs/input";
    }
}

} // namespace blockchain
} // namespace libbitcoin
