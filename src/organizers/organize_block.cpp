/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/organizers/organize_block.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;
using namespace bc::system::chain;
using namespace bc::system::config;
using namespace std::placeholders;

#define NAME "organize_block"

organize_block::organize_block(prioritized_mutex& mutex,
    dispatcher& priority_dispatch, threadpool& threads, fast_chain& chain,
    block_pool& pool, const settings& settings,
    const system::settings& bitcoin_settings)
  : fast_chain_(chain),
    mutex_(mutex),
    stopped_(true),
    pool_(pool),
    dispatch_(priority_dispatch),
    validator_(priority_dispatch, chain, settings, bitcoin_settings),
    downloader_subscriber_(std::make_shared<download_subscriber>(threads, NAME))
{
}

// Properties.
//-----------------------------------------------------------------------------

bool organize_block::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool organize_block::start()
{
    stopped_ = false;
    pool_.start();
    downloader_subscriber_->start();
    validator_.start();

    // Each download queues up a validation sequence.
    downloader_subscriber_->subscribe(
        std::bind(&organize_block::handle_check,
            this, _1, _2), error::service_stopped, 0);

    return true;
}

bool organize_block::stop()
{
    validator_.stop();
    downloader_subscriber_->stop();
    downloader_subscriber_->invoke(error::service_stopped, 0);
    pool_.stop();
    stopped_ = true;
    return true;
}

// Organize.
//-----------------------------------------------------------------------------

code organize_block::organize(block_const_ptr block, size_t height)
{
    // Check duration set here.
    // Checks that are independent of chain state (header, block, txs).
    validator_.check(block, height);

    // Associate duration set here.
    // Store txs (if missing) and associate them to candidate block.
    // Existing txs cannot suffer a state change as they may also be confirmed.
    //#########################################################################
    const auto error_code = fast_chain_.update(block, height);
    //#########################################################################

    // Failure code implies store corruption, caller should log.
    if (error_code)
        return error_code;

    // Add to the block download cache as applicable.
    pool_.add(block, height);

    // Queue download notification to invoke validation on downloader thread.
    downloader_subscriber_->relay(error_code, height);

    // Validation result is returned by metadata.error.
    return error::success;
}

// TODO: refactor to eliminate this interface abstraction leak.
void organize_block::prime_validation(size_t height) const
{
    downloader_subscriber_->relay(error::success, height);
}

// Validate sequence.
//-----------------------------------------------------------------------------
// This runs in single thread normal priority except validation fan-outs.
// Therefore fan-outs may use all threads in the priority threadpool.

// This is the start of the validation sequence.
// TODO: can confirm in parallel if not validating or indexing, and no gaps.
bool organize_block::handle_check(const code& ec, size_t height)
{
    BITCOIN_ASSERT(!ec || ec == error::service_stopped);

    if (ec)
        return false;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_high_priority();

    const auto parent_state = fast_chain_.top_valid_candidate_state();

    // If aligned there is a new next populated candidate to validate.
    if (height - 1u != parent_state->height())
    {
        //---------------------------------------------------------------------
        mutex_.unlock_high_priority();
        return true;
    }

    // Start a (potentially total) sub-branch of the current reorg attempt.
    const auto sub_branch = std::make_shared<block_const_ptr_list>();

    // TODO: use a scope lock to release the critical section.
    // TODO: templatize scope_lock to allow for priority mutex.
    const result_handler complete =
        std::bind(&organize_block::handle_complete,
            this, _1);

    // Get the candidate from the pool (or read it from the store when able).
    pool_.fetch(height,
        std::bind(&organize_block::handle_fetch,
            this, _1, _2, height, parent_state->hash(), sub_branch, complete));

    return true;
}

// private
void organize_block::handle_complete(const code& ec)
{
    mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////

    if (ec && ec != error::service_stopped)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure in block organization, store is now corrupt: "
            << ec.message();
    }
}

void organize_block::handle_fetch(const code& ec, block_const_ptr block,
    size_t height, const hash_digest& parent_hash,
    block_const_ptr_list_ptr sub_branch, result_handler complete)
{
    code error_code;

    if (ec)
    {
        complete(ec);
        return;
    }

    // It is possible that the block will no longer be a candidate in which
    // case block will be empty. This is okay as it implies there is a
    // candidate branch forked at or below this height pending processing.
    // If the block doesn't match there was a reorg and the new block of the
    // same height must eventually be downloaded and validation triggered.
    if (!block || block->header().previous_block_hash() != parent_hash)
    {
        // These blocks will be queried and confirmed later in reorganize.
        // This happens when there are gaps in the populated candidate chain.
        if (!sub_branch->empty())
        {
            // Add all blocks from sub_branch to cache.
            pool_.add(sub_branch, height - sub_branch->size());

            LOG_VERBOSE(LOG_BLOCKCHAIN)
                << sub_branch->size() << " blocks validated.";
        }

        complete(ec);
        return;
    }

    // Checks that are dependent upon chain state.
    // Populate, accept, and connect durations set here.
    // Deserialization duration and median_time_past set by read.
    if ((error_code = validate(block)))
    {
        complete(error_code);
        return;
    }

    // CONSENSUS: TODO: handle invalidity caching of merkle mutations.
    if (block->header().metadata.error)
    {
        // This triggers BLOCK reorganization notifications.
        // Pop and mark as invalid candidates at and above block.
        //#################################################################
        error_code = fast_chain_.invalidate(block, height);
        //#################################################################

        complete(error_code);
        return;
    }

    // TODO: don't candidate block if branch would be reorganizable,
    // TODO: just move straight to reorganization. This will always
    // TODO: skip candidizing the last block before reorg.

    // This triggers NO notifications.
    // Mark candidate block as valid and mark candidate-spent outputs.
    //#################################################################
    error_code = fast_chain_.candidate(block);
    //#################################################################

    if (error_code)
    {
        complete(error_code);
        return;
    }

    // Add the valid candidate to the candidate sub-branch.
    sub_branch->push_back(block);

    if (fast_chain_.is_reorganizable())
    {
        const auto branch_height = height - (sub_branch->size() - 1u);

        // This triggers BLOCK reorganization notifications.
        // Reorganize this stronger candidate branch into confirmed chain.
        //#################################################################
        error_code = fast_chain_.reorganize(sub_branch, branch_height);
        //#################################################################

        // Reset the branch for next reorganization.
        sub_branch->clear();
    }

    if (error_code)
    {
        complete(error_code);
        return;
    }

    // Break recursion using concurrent dispatch.
    dispatch_.concurrent(&organize_block::block_fetcher, this,
        ++height, block->hash(), sub_branch, complete);
}

// private
void organize_block::block_fetcher(size_t height,
    const hash_digest& parent_hash, block_const_ptr_list_ptr sub_branch,
    result_handler complete)
{
    pool_.fetch(height,
        std::bind(&organize_block::handle_fetch,
            this, _1, _2, height, parent_hash, sub_branch, complete));
}

// Validate sub-sequence.
//-----------------------------------------------------------------------------

// private
// Convert validate.accept/connect to a sequential call.
code organize_block::validate(block_const_ptr block)
{
    resume_ = {};

    const result_handler complete =
        std::bind(&organize_block::signal_completion,
            this, _1);

    const auto accept_handler =
        std::bind(&organize_block::handle_accept,
            this, _1, block, complete);

    // Checks that are dependent upon chain state.
    validator_.accept(block, accept_handler);

    // Store failed or received stop code from validator.
    return resume_.get_future().get();
}

// private
void organize_block::signal_completion(const code& ec)
{
    resume_.set_value(ec);
}

// private
void organize_block::handle_accept(const code& ec, block_const_ptr block,
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
        std::bind(&organize_block::handle_connect,
            this, _1, block, handler);

    // Checks that include script metadata.
    validator_.connect(block, connect_handler);
}

// private
void organize_block::handle_connect(const code& ec, block_const_ptr,
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
