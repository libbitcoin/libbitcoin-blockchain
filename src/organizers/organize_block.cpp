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
    downloader_subscriber_->start();
    validator_.start();

    // Each download queues up a validation sequence.
    downloader_subscriber_->subscribe(
        std::bind(&organize_block::handle_check,
            this, _1, _2, _3), error::service_stopped, {}, 0);

    return true;
}

bool organize_block::stop()
{
    validator_.stop();
    downloader_subscriber_->stop();
    downloader_subscriber_->invoke(error::service_stopped, {}, 0);
    pool_.clear();
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

    // Add to the block download cache.
    ////pool_.add(block, height);

    // Queue download notification to invoke validation on downloader thread.
    downloader_subscriber_->relay(error_code, block->hash(), height);

    // Validation result is returned by metadata.error.
    return error::success;
}

// TODO: refactor to eliminate this abstraction leak.
void organize_block::prime_validation(const hash_digest& hash,
    size_t height) const
{
    downloader_subscriber_->relay(error::success, hash, height);
}

// Validate sequence.
//-----------------------------------------------------------------------------
// This runs in single thread normal priority except validation fan-outs.
// Therefore fan-outs may use all threads in the priority threadpool.

// This is the start of the validation sequence.
bool organize_block::handle_check(const code& ec, const hash_digest& hash,
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
    code error_code;

    for (;!stopped() && height != 0; ++height)
    {
        // Deserialization duration and median_time_past set here.
        const auto block = fast_chain_.get_candidate(height);

        // TODO: move pruning.
        pool_.prune(height);

        // If not next block must be a post-reorg expired notification.
        if (!block || fast_chain_.top_valid_candidate_state()->hash() !=
            block->header().previous_block_hash())
            break;

        // Checks that are dependent upon chain state.
        // Populate, accept, and connect durations set here.
        if ((error_code = validate(block)))
            break;

        if (block->header().metadata.error)
        {
            // CONSENSUS: TODO: handle invalidity caching of merkle mutations.

            // This triggers BLOCK reorganization notifications.
            // Pop and mark as invalid candidates at and above block.
            //#################################################################
            error_code = fast_chain_.invalidate(block, height);
            //#################################################################

            // Candidate chain is invalid at and above this point so break out.
            break;
        }
        else
        {
            // TODO: don't candidate block if branch would be reorganizable,
            // TODO: just move straight to reorganization. This will always
            // TODO: skip candidizing the last block before reorg.

            // This triggers NO notifications.
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
            const auto branch_height = height - (branch_cache->size() - 1u);

            // This triggers BLOCK reorganization notifications.
            // Reorganize this stronger candidate branch into confirmed chain.
            //#################################################################
            error_code = fast_chain_.reorganize(branch_cache, branch_height);
            //#################################################################

            if (error_code)
                break;

            // Reset the branch for next reorganization.
            branch_cache->clear();
        }
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

    // If not reorganized now these blocks will be queried and confirmed later.
    if (!branch_cache->empty())
    {
        LOG_DEBUG(LOG_BLOCKCHAIN)
            << branch_cache->size() <<
            " blocks validated but not yet organized.";
    }

    // Resubscribe if not stop or store failure.
    // In the case of a store failure the server will stop processing blocks.
    return !error_code;
}

// private
// Convert validate.accept/connect to a sequential call.
code organize_block::validate(block_const_ptr block)
{
    resume_ = std::promise<code>();

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

// Verify sub-sequence.
//-----------------------------------------------------------------------------

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
