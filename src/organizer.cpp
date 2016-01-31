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
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/block.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <system_error>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block_chain.hpp>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/orphan_pool.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>

INITIALIZE_TRACK(bc::blockchain::organizer::reorganize_subscriber);

namespace libbitcoin {
namespace blockchain {

#define NAME "organizer"

organizer::organizer(threadpool& pool, orphan_pool& orphans,
    simple_chain& chain)
  : stopped_(false),
    orphans_(orphans),
    chain_(chain),
    subscriber_(std::make_shared<reorganize_subscriber>(pool, NAME))
{
}

// This is called on *every* blockchain_impl::do_store() call.
void organizer::organize()
{
    // Load unprocessed blocks
    process_queue_ = orphans_.unprocessed();

    // As we loop, we pop blocks off and process them
    while (!process_queue_.empty() && !stopped())
    {
        const auto process_block = process_queue_.back();
        process_queue_.pop_back();

        // process() can remove blocks from the queue too
        process(process_block);
    }
}

// The subscriber is not restartable.
void organizer::stop()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    std::lock_guard<std::mutex> lock(mutex_);

    // stopped_/subscriber_ is the guarded relation.
    notify_stop();
    stopped_ = true;
    ///////////////////////////////////////////////////////////////////////////
}

bool organizer::stopped()
{
    return stopped_;
}

void organizer::process(block_detail::ptr process_block)
{
    BITCOIN_ASSERT(process_block);

    // Trace the chain in the orphan pool
    auto orphan_chain = orphans_.trace(process_block);
    BITCOIN_ASSERT(orphan_chain.size() >= 1);

    const auto& hash = orphan_chain[0]->actual().header.previous_block_hash;

    uint64_t fork_index;
    if (chain_.find_height(fork_index, hash))
        replace_chain(fork_index, orphan_chain);

    // Don't mark all orphan_chain as processed here because there might be
    // a winning fork from an earlier block
    process_block->mark_processed();
}

void organizer::replace_chain(uint64_t fork_index,
    block_detail::list& orphan_chain)
{
    hash_number orphan_work = 0;
    for (uint64_t orphan = 0; orphan < orphan_chain.size(); ++orphan)
    {
        const auto ec = verify(fork_index, orphan_chain, orphan);
        if (ec)
        {
            // If invalid for error::checkpoints_failed we should drop the
            // connection, but there is no reference from here.
            if (ec != error::service_stopped)
            {
                const auto& header = orphan_chain[orphan]->actual().header;
                const auto block_hash = encode_hash(header.hash());
                log::warning(LOG_VALIDATE)
                    << "Invalid block [" << block_hash << "] " << ec.message();
            }

            // Block is invalid, clip the orphans.
            clip_orphans(orphan_chain, orphan, ec);

            // Stop summing work once we discover an invalid block
            break;
        }

        const auto& orphan_block = orphan_chain[orphan]->actual();
        orphan_work += block_work(orphan_block.header.bits);
    }

    // All remaining blocks in orphan_chain should all be valid now
    // Compare the difficulty of the 2 forks (original and orphan)
    const auto begin_index = fork_index + 1;
    const auto main_work = chain_.sum_difficulty(begin_index);
    if (orphan_work <= main_work)
        return;

    // Replace! Switch!
    block_detail::list released_blocks;
    DEBUG_ONLY(bool success =) chain_.release(begin_index, released_blocks);
    BITCOIN_ASSERT(success);

    if (!released_blocks.empty())
        log::warning(LOG_BLOCKCHAIN) << "Reorganizing blockchain ["
            << begin_index << ", " << released_blocks.size() << "]";

    // We add the arriving blocks first to the main chain because if
    // we add the blocks being replaced back to the pool first then
    // the we can push the arrival blocks off the bottom of the
    // circular buffer.
    // Then when we try to remove the block from the orphans pool,
    // if will fail to find it. I would rather not add an exception
    // there so that problems will show earlier.
    // All arrival_blocks should be blocks from the pool.
    auto arrival_index = fork_index;
    for (const auto arrival_block: orphan_chain)
    {
        orphans_.remove(arrival_block);
        ++arrival_index;
        arrival_block->set_info({ block_status::confirmed, arrival_index });
        chain_.append(arrival_block);
    }

    // Now add the old blocks back to the pool
    for (const auto replaced_block: released_blocks)
    {
        static constexpr uint64_t no_height = 0;
        replaced_block->mark_processed();
        replaced_block->set_info({ block_status::orphan, no_height });
        orphans_.add(replaced_block);
    }

    notify_reorganize(fork_index, orphan_chain, released_blocks);
}

static void lazy_remove(block_detail::list& process_queue,
    block_detail::ptr remove_block)
{
    BITCOIN_ASSERT(remove_block);
    const auto it = std::find(process_queue.begin(), process_queue.end(),
        remove_block);
    if (it != process_queue.end())
        process_queue.erase(it);

    remove_block->mark_processed();
}

void organizer::clip_orphans(block_detail::list& orphan_chain,
    uint64_t orphan_index, const code& invalid_reason)
{
    // Remove from orphans pool.
    auto orphan_start = orphan_chain.begin() + orphan_index;
    for (auto it = orphan_start; it != orphan_chain.end(); ++it)
    {
        if (it == orphan_start)
            (*it)->set_error(invalid_reason);
        else
            (*it)->set_error(error::previous_block_invalid);

        static const uint64_t no_height = 0;
        const block_info info{ block_status::rejected, no_height };
        (*it)->set_info(info);
        orphans_.remove(*it);

        // Also erase from process_queue so we avoid trying to re-process
        // invalid blocks and remove try to remove non-existant blocks.
        lazy_remove(process_queue_, *it);
    }

    orphan_chain.erase(orphan_start, orphan_chain.end());
}

void organizer::subscribe_reorganize(block_chain::reorganize_handler handler)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    if (true)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!stopped())
        {
            subscriber_->subscribe(handler);
            return;
        }
    }
    ///////////////////////////////////////////////////////////////////////////

    handler(error::service_stopped, 0, {}, {});
}

void organizer::notify_reorganize(uint64_t fork_point,
    const block_detail::list& orphan_chain,
    const block_detail::list& replaced_chain)
{
    const auto to_raw_pointer = [](const block_detail::ptr& detail)
    {
        return detail->actual_ptr();
    };

    block_chain::list arrivals(orphan_chain.size());
    std::transform(orphan_chain.begin(), orphan_chain.end(),
        arrivals.begin(), to_raw_pointer);

    block_chain::list replacements(replaced_chain.size());
    std::transform(replaced_chain.begin(), replaced_chain.end(),
        replacements.begin(), to_raw_pointer);

    subscriber_->relay(error::success, fork_point, arrivals, replacements);
}

void organizer::notify_stop()
{
    subscriber_->stop();
    subscriber_->relay(error::service_stopped, 0, {}, {});
}

} // namespace blockchain
} // namespace libbitcoin
