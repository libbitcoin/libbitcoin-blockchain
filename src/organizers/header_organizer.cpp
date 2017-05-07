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
#include <bitcoin/blockchain/organizers/header_organizer.hpp>

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_header.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "header_organizer"

// Database access is limited to:
// block: { bits, version, timestamp }

header_organizer::header_organizer(prioritized_mutex& mutex, dispatcher& dispatch,
    threadpool&, fast_chain& chain, const settings& settings)
  : mutex_(mutex),
    stopped_(true),
    validator_(dispatch, chain, settings)
{
}

// Properties.
//-----------------------------------------------------------------------------

bool header_organizer::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool header_organizer::start()
{
    stopped_ = false;
    validator_.start();
    return true;
}

bool header_organizer::stop()
{
    validator_.stop();
    stopped_ = true;
    return true;
}

// Organize sequence.
//-----------------------------------------------------------------------------

// This is called from block_chain::organize.
void header_organizer::organize(header_const_ptr header,
    result_handler handler)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_high_priority();

    // Reset the reusable promise.
    resume_ = std::promise<code>();

    const result_handler complete =
        std::bind(&header_organizer::signal_completion,
            this, _1);

    const auto check_handler =
        std::bind(&header_organizer::handle_check,
            this, _1, header, complete);

    // Checks that are independent of chain state.
    validator_.check(header, check_handler);

    // Wait on completion signal.
    // This is necessary in order to continue on a non-priority thread.
    // If we do not wait on the original thread there may be none left.
    auto ec = resume_.get_future().get();

    mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////

    // Invoke caller handler outside of critical section.
    handler(ec);
}

// private
void header_organizer::signal_completion(const code& ec)
{
    // This must be protected so that it is properly cleared.
    // Signal completion, which results in original handler invoke with code.
    resume_.set_value(ec);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// private
void header_organizer::handle_check(const code& ec, header_const_ptr header,
    result_handler handler)
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

    const auto accept_handler =
        std::bind(&header_organizer::handle_accept,
            this, _1, header, handler);

    // Checks that are dependent on chain state.
    validator_.accept(header, accept_handler);
}

// private
void header_organizer::handle_accept(const code& ec, header_const_ptr header,
    result_handler handler)
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

    //=========================================================================
    //=========================================================================
    // TODO: add header to the store (unconfirmed, empty).
    //=========================================================================
    //=========================================================================
    handler(error::success);
}

// Queries.
//-----------------------------------------------------------------------------

// TODO: once headers are stubbed in the store this moves into store also.
void header_organizer::filter(get_data_ptr message) const
{
    // TODO: implement in the header pool.
    ////header_pool_.filter(message);
}

} // namespace blockchain
} // namespace libbitcoin
