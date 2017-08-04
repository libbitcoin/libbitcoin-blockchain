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
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_header.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace bc::database;
using namespace std::placeholders;

#define NAME "header_organizer"

// Database access is limited to { bits, version, timestamp }.

header_organizer::header_organizer(prioritized_mutex& mutex,
    dispatcher& dispatch, threadpool& thread_pool, fast_chain& chain,
    const settings& settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    dispatch_(dispatch),
    header_pool_(settings.reorganization_limit),
    validator_(dispatch, chain, settings),
    subscriber_(std::make_shared<reindex_subscriber>(thread_pool, NAME))
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
    subscriber_->start();
    validator_.start();
    return true;
}

bool header_organizer::stop()
{
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, 0, {}, {});
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

    const result_handler complete =
        std::bind(&header_organizer::handle_complete,
            this, _1, handler);

    const auto check_handler =
        std::bind(&header_organizer::handle_check,
            this, _1, header, complete);

    // Checks that are independent of chain state.
    validator_.check(header, check_handler);
}

// private
void header_organizer::handle_complete(const code& ec, result_handler handler)
{
    mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////

    // Invoke caller handler outside of critical section.
    handler(ec);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// private
//*****************************************************************************
// CONSENSUS: Hash existence is the same check performed by satoshi, yet it
// will produce a chain split in the case of a hash collision. This is because
// it is not applied at the branch point, so some nodes will not see the
// collision block and others will, depending on block order of arrival.
// The hash collision is explicitly ignored for block hashes.
//*****************************************************************************
void header_organizer::handle_check(const code& ec, header_const_ptr header,
    result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    const auto branch = header_pool_.get_branch(header);

    const auto accept_handler =
        std::bind(&header_organizer::handle_accept,
            this, _1, branch, handler);

    // Checks that are dependent on chain state.
    validator_.accept(branch, accept_handler);
}

// private
void header_organizer::handle_accept(const code& ec, header_branch::ptr branch,
    result_handler handler)
{
    // A header-pooled (duplicate) header is returned here.
    // An existing (duplicate) indexed or confirmed header is returned here.
    // A stored but invalid block is returned with validation code here.
    // A pooled header skips validation and only updates on store.
    if (ec)
    {
        handler(ec);
        return;
    }

    // The top block is valid even if the branch has insufficient work.
    const auto top = branch->top();
    top->validation.height = branch->top_height();

    // TODO: create a simulated validation path that does not block others.
    if (top->validation.simulate)
    {
        handler(error::success);
        return;
    }

    const auto work = branch->work();
    uint256_t required;

    // This stops before the height or at the work level, which ever is first.
    if (!fast_chain_.get_work(required, work, branch->height(), false))
    {
        handler(error::operation_failed);
        return;
    }

    if (work <= required)
    {
        // Pool.add clears parent header state to preserve memory.
        header_pool_.add(top);
        handler(error::insufficient_work);
        return;
    }

    // Get the outgoing headers to forward to reorg handler.
    const auto outgoing = std::make_shared<header_const_ptr_list>();

    const auto reindexed_handler =
        std::bind(&header_organizer::handle_reindexed,
            this, _1, branch, outgoing, handler);

    // Replace! Switch!
    //#########################################################################
    fast_chain_.reindex(branch->fork_point(), branch->headers(), outgoing,
        dispatch_, reindexed_handler);
    //#########################################################################
}

// private
void header_organizer::handle_reindexed(const code& ec,
    header_branch::const_ptr branch, header_const_ptr_list_ptr outgoing,
    result_handler handler)
{
    if (ec)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing header to store, is now corrupted: "
            << ec.message();
        handler(ec);
        return;
    }

    header_pool_.remove(branch->headers());
    header_pool_.prune(branch->top_height());
    header_pool_.add(outgoing);

    notify(branch->height(), branch->headers(), outgoing);
    handler(error::success);
}

// Subscription.
//-----------------------------------------------------------------------------

// private
void header_organizer::notify(size_t fork_height,
    header_const_ptr_list_const_ptr incoming,
    header_const_ptr_list_const_ptr outgoing)
{
    // This invokes handlers within the criticial section (deadlock risk).
    subscriber_->invoke(error::success, fork_height, incoming, outgoing);
}

// This allows subscriber to become aware of pending and obsolete downloads.
void header_organizer::subscribe(reindex_handler&& handler)
{
    subscriber_->subscribe(std::move(handler),
        error::service_stopped, 0, {}, {});
}

void header_organizer::unsubscribe()
{
    subscriber_->relay(error::success, 0, {}, {});
}

// Queries.
//-----------------------------------------------------------------------------

void header_organizer::filter(get_data_ptr message) const
{
    header_pool_.filter(message);
}

} // namespace blockchain
} // namespace libbitcoin
