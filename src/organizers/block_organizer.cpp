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
#include <bitcoin/blockchain/organizers/block_organizer.hpp>

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "block_organizer"

// TODO: ensure intended thread prioritization.
block_organizer::block_organizer(prioritized_mutex& mutex,
    dispatcher& dispatch, threadpool& thread_pool, fast_chain& chain,
    const settings& settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    dispatch_(dispatch),
    validator_(dispatch, chain, settings),
    downloader_(std::make_shared<download_subscriber>(thread_pool, NAME)),
    subscriber_(std::make_shared<reorganize_subscriber>(thread_pool, NAME))
{
}

// Properties.
//-----------------------------------------------------------------------------

bool block_organizer::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool block_organizer::start()
{
    stopped_ = false;
    downloader_->start();
    subscriber_->start();
    validator_.start();

    auto checked =
        std::bind(&block_organizer::handle_check,
            this, _1, _2, _3);

    // Each download initiates the validation sequence.
    downloader_->subscribe(std::move(checked), error::service_stopped, {}, 0);
    return true;
}

bool block_organizer::stop()
{
    validator_.stop();
    downloader_->stop();
    downloader_->invoke(error::service_stopped, {}, 0);
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, 0, {}, {});
    stopped_ = true;
    return true;
}

// Organize.
//-----------------------------------------------------------------------------

// This operates on a caller (network) thread, concurrent execution.
// Unlike with header and tx organizers, block.organize() is synchronous.
code block_organizer::organize(block_const_ptr block, size_t height)
{
    code error_code;

    // TODO: exclude checks if under checkpoint or milestone (optimization).
    // TODO: maintain state for top encountered header milestone above valid.

    // Checks that are independent of chain state (header, block, txs).
    if (validator_.check(block))
    {
        // Store txs (if missing) and associate them to candidate block.
        //#####################################################################
        error_code = fast_chain_.update(block, height);
        //#####################################################################
    }

    // If block is invalid headers will be popped.
    // Downloader notifications are private to the organizer, so send here.
    // Post download notification to invoke the validator (on priority thread).
    downloader_->invoke(error_code, block, height);

    // Validation result is returned by metadata.error.
    // Failure code implies store corruption, caller should log.
    return error_code;
}

// Validate sequence.
//-----------------------------------------------------------------------------

// This is the start of the validation sequence.
bool block_organizer::handle_check(const code& ec, block_const_ptr block,
    size_t height)
{
    if (ec)
        return false;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_high_priority();

    // If initial height is misaligned try again on next download.
    if (height != fast_chain_.top_valid_candidate_state()->height() + 1u)
        return true;

    // Stack up the validated blocks for possible reorganization.
    auto branch_cache = std::make_shared<block_const_ptr_list>();
    const auto branch_height = height;
    code error_code;

    while (!stopped() && block)
    {
        const result_handler complete =
            std::bind(&block_organizer::signal_completion,
                this, _1);

        const auto accept_handler =
            std::bind(&block_organizer::handle_accept,
                this, _1, block, complete);

        // Perform contextual block validation.
        resume_ = std::promise<code>();
        validator_.accept(block, accept_handler);

        if ((error_code = resume_.get_future().get()))
        {
            // Store failure or stop code from validator.
            break;
        }

        if (block->header().metadata.error)
        {
            // Pop and mark as invalid candidates at and above block.
            //#################################################################
            error_code = fast_chain_.invalidate(block, height);
            //#################################################################

            // Candidate chain is invalid at this point so break here.
            break;
        }
        else
        {
            // Mark candidate block as valid and mark candidate-spent outputs.
            //#################################################################
            error_code = fast_chain_.candidate(block);
            //#################################################################
            branch_cache->push_back(block);

            if (error_code)
                break;
        }

        if (fast_chain_.is_reorganizable())
        {
            // Reorganize this stronger candidate branch into confirmed chain.
            //#################################################################
            error_code = fast_chain_.reorganize(branch_cache, branch_height);
            //#################################################################
            branch_cache->clear();

            if (error_code)
                break;
        }

        // TODO: use parallel block reader (this is expensive and serial).
        block = fast_chain_.get_block(++height, true, false);
    }

    mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////

    // A non-stop error result implies store corruption.
    if (error_code && error_code != error::service_stopped)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure in block organization, store is now corrupt: "
            << ec.message();
    }

    // Resubscribe if not stop or store failure.
    return !error_code;
}

// private
void block_organizer::signal_completion(const code& ec)
{
    // This must be protected so that it is properly cleared.
    // Signal completion, which results in original handler invoke with code.
    resume_.set_value(ec);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// private
void block_organizer::handle_accept(const code& ec, block_const_ptr block,
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

    const auto connect_handler =
        std::bind(&block_organizer::handle_connect,
            this, _1, block, handler);

    // Checks that include script metadata.
    validator_.connect(block, connect_handler);
}

// private
void block_organizer::handle_connect(const code& ec, block_const_ptr block,
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

    handler(error::success);
}

////// private
////// Outgoing blocks must have median_time_past set.
////void block_organizer::handle_reorganized(const code& ec,
////    block_const_ptr_list_const_ptr incoming,
////    block_const_ptr_list_ptr outgoing, result_handler handler)
////{
////    if (ec)
////    {
////        LOG_FATAL(LOG_BLOCKCHAIN)
////            << "Failure writing block to store, is now corrupted: "
////            << ec.message();
////        handler(ec);
////        return;
////    }
////
////    ////// v3 reorg block order is reverse of v2, branch.back() is the new top.
////    ////notify(branch->height(), branch->blocks(), outgoing);
////
////    handler(error::success);
////}

// Subscription.
//-----------------------------------------------------------------------------

// private
void block_organizer::notify(size_t fork_height,
    block_const_ptr_list_const_ptr incoming,
    block_const_ptr_list_const_ptr outgoing)
{
    // This invokes handlers within the criticial section (deadlock risk).
    subscriber_->invoke(error::success, fork_height, incoming, outgoing);
}

void block_organizer::subscribe(reorganize_handler&& handler)
{
    subscriber_->subscribe(std::move(handler),
        error::service_stopped, 0, {}, {});
}

void block_organizer::unsubscribe()
{
    subscriber_->relay(error::success, 0, {}, {});
}

} // namespace blockchain
} // namespace libbitcoin
