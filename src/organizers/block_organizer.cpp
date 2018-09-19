/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "block_organizer"

block_organizer::block_organizer(prioritized_mutex& mutex,
    dispatcher& priority_dispatch, threadpool& threads, fast_chain& chain,
    const settings& settings, const bc::settings& bitcoin_settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    validator_(priority_dispatch, chain, settings, bitcoin_settings),
    downloader_subscriber_(std::make_shared<download_subscriber>(threads, NAME))
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
    downloader_subscriber_->start();
    validator_.start();

    // Each download queues up a validation sequence.
    downloader_subscriber_->subscribe(
        std::bind(&block_organizer::handle_check,
            this, _1, _2, _3), error::service_stopped, {}, 0);

    return true;
}

bool block_organizer::stop()
{
    validator_.stop();
    downloader_subscriber_->stop();
    downloader_subscriber_->invoke(error::service_stopped, {}, 0);
    stopped_ = true;
    return true;
}

// Organize.
//-----------------------------------------------------------------------------

code block_organizer::organize(block_const_ptr block, size_t height)
{
    // Checks that are independent of chain state (header, block, txs).
    validator_.check(block, height);

    // Store txs (if missing) and associate them to candidate block.
    // Existing txs cannot suffer a state change as they may also be confirmed.
    //#########################################################################
    const auto error_code = fast_chain_.update(block, height);
    //#########################################################################

    // TODO: cache block as last downloaded (for fast top validation).
    // Queue download notification to invoke validation on downloader thread.
    downloader_subscriber_->relay(error_code, block->hash(), height);

    // Validation result is returned by metadata.error.
    // Failure code implies store corruption, caller should log.
    return error_code;
}

// TODO: refactor to eliminate this abstraction leak.
void block_organizer::prime_validation(const hash_digest& hash,
    size_t height) const
{
    downloader_subscriber_->relay(error::success, hash, height);
}

// Validate sequence.
//-----------------------------------------------------------------------------
// This runs in single thread normal priority except validation fan-outs.
// Therefore fan-outs may use all threads in the priority threadpool.

// This is the start of the validation sequence.
bool block_organizer::handle_check(const code& ec, const hash_digest& hash,
    size_t height)
{
    if (ec)
        return false;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_high_priority();

    // If initial height is misaligned try again on next download.
    if (fast_chain_.top_valid_candidate_state()->height() != height - 1u)
    {
        //---------------------------------------------------------------------
        mutex_.unlock_high_priority();
        return true;
    }

    // Stack up the validated blocks for possible reorganization.
    auto branch_cache = std::make_shared<block_const_ptr_list>();
    auto branch_height = height;
    code error_code;

    for (auto branch_height = height; !stopped() && height != 0; ++height)
    {
        // TODO: check last downloaded cache first (for fast top validation).
        // TODO: create parallel block reader (this is expensive and serial).
        // TODO: this can run in the block populator using priority dispatch.
        // TODO: consider metadata population in line with block read.
        auto block = fast_chain_.get_block(height, true, true);

        LOG_DEBUG(LOG_BLOCKCHAIN)
            << "Got next block #" << height;

        // If hash is misaligned we must be looking at an expired notification.
        if (!block || fast_chain_.top_valid_candidate_state()->hash() !=
            block->header().previous_block_hash())
            break;

        // Checks that are dependent upon chain state.
        if ((error_code = validate(block)))
            break;

        if (block->header().metadata.error)
        {
            // TODO: handle invalidity caching of merkle mutations.
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

        LOG_DEBUG(LOG_BLOCKCHAIN)
            << "Validated block #" << height << " ["
            << encode_hash(block->hash()) << "]";

        if (fast_chain_.is_reorganizable())
        {
            // Reorganize this stronger candidate branch into confirmed chain.
            //#################################################################
            error_code = fast_chain_.reorganize(branch_cache, branch_height);
            //#################################################################

            if (error_code)
                break;

            LOG_INFO(LOG_BLOCKCHAIN)
                << "Organized blocks [" << branch_height << "-"
                << branch_height + branch_cache->size() - 1u << "]";

            // Reset the branch for next reorganization.
            branch_height += branch_cache->size();
            branch_cache->clear();
        }

        // Top valid chain state should have been updated to match the block.
        BITCOIN_ASSERT(fast_chain_.top_valid_candidate_state()->height() ==
            height);
    }

    mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////

    // A non-stop error result implies store corruption.
    if (error_code && error_code != error::service_stopped)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure in block organization, store is now corrupt: "
            << error_code.message();
    }

    // Resubscribe if not stop or store failure.
    // In the case of a store failure the server will stop processing blocks.
    return !error_code;
}

// private
// Convert validate.accept/connect to a sequential call.
code block_organizer::validate(block_const_ptr block)
{
    resume_ = std::promise<code>();

    const result_handler complete =
        std::bind(&block_organizer::signal_completion,
            this, _1);

    const auto accept_handler =
        std::bind(&block_organizer::handle_accept,
            this, _1, block, complete);

    // Checks that are dependent upon chain state.
    validator_.accept(block, accept_handler);

    // Store failed or received stop code from validator.
    return resume_.get_future().get();
}

// private
void block_organizer::signal_completion(const code& ec)
{
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
void block_organizer::handle_connect(const code& ec, block_const_ptr,
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

} // namespace blockchain
} // namespace libbitcoin
