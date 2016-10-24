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

// bool activated = activatedheight >= full_activation_height_;

// TODO: look into threadpool fallback initialization order (pool empty).
// Fall back to the metwork pool if zero threads in priority pool.
validate_block::validate_block(threadpool& pool, const fast_chain& chain,
    const settings& settings)
  : stopped_(false),
    use_libconsensus_(settings.use_libconsensus),
    priority_pool_(settings.threads, settings.priority ?
        thread_priority::high : thread_priority::normal),
    thread_pool_(/*settings.threads == 0 ? pool : */ priority_pool_),
    dispatch_(thread_pool_, NAME "_dispatch"),
    populator_(thread_pool_, chain, settings)
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
    ////// Static checks are never skipped.
    ////const auto start_time = asio::steady_clock::now();
    ////const auto ec = block->check();
    ////report(block, start_time, ec ? "UNCHECKED" : "checked  ");
    ////return ec;

    // The above includes no height and the cost is tiny, so not reporting.
    return block->check();
}

// Accept sequence.
//-----------------------------------------------------------------------------
// These checks require height or other chain context.

void validate_block::accept(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    BITCOIN_ASSERT(index < fork->size());

    const auto block = fork->block_at(index);
    const auto height = fork->height_at(index);

    const result_handler complete_handler =
        std::bind(&validate_block::handle_accepted,
            this, _1, block, height, asio::steady_clock::now(), handler);

    // Skip data and set population if both are populated.
    if (block->validation.state && block->validation.sets)
    {
        complete_handler(error::success);
        return;
    }

    populator_.populate(fork, index, complete_handler);
}

void validate_block::handle_accepted(const code& ec, block_const_ptr block,
    size_t height, asio::time_point start_time, result_handler handler) const
{
    // If population errs there may not be validation state.
    const auto skipped = ec || !block->validation.state->use_full_validation();

    if (skipped)
    {
        handler(ec);
        return;
    }

    report(block, start_time, ec ? "UNPOPULATED" : "populated");

    // Since population was successful and not skipped run the accept checks.
    const auto accept_start_time = asio::steady_clock::now();
    const auto error_code = block->accept();
    report(block, accept_start_time, error_code ? "REJECTED  " : "accepted ");
    handler(error_code);
}

// Connect sequence.
//-----------------------------------------------------------------------------
// These checks require output traversal and validation.

void validate_block::connect(fork::const_ptr fork, size_t index,
    result_handler handler) const
{
    BITCOIN_ASSERT(index < fork->size());

    const auto block = fork->block_at(index);
    const auto height = fork->height_at(index);
    const auto& txs = block->transactions();
    const auto sets = block->validation.sets;
    const auto state = block->validation.state;

    // Data and set population must be completed via a prior call to accept.
    // If !use_full_validation are to be skipped this should not be called.
    if (!sets || !state)
    {
        handler(error::operation_failed);
        return;
    }

    const result_handler complete_handler =
        std::bind(&validate_block::handle_connected,
            this, _1, block, height, asio::steady_clock::now(), handler);

    // Sets will be empty if there is only a coinbase tx.
    if (sets->empty() || !state->use_full_validation())
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
    size_t height, asio::time_point start_time, result_handler handler) const
{
    const auto skipped = ec || !block->validation.state->use_full_validation();

    if (skipped)
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
    const auto modulus =
        (height < 100000 ? 1000 :
        (height < 200000 ? 100 :
        (height < 300000 ? 10 : 1)));

    return height % modulus == 0;
}

void validate_block::report(block_const_ptr block, asio::time_point start_time,
    const std::string& token)
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
