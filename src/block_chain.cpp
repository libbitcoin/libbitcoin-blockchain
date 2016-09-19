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
#include <bitcoin/blockchain/block_chain.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/block_fetcher.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>

namespace libbitcoin {
namespace blockchain {

////#define NAME "blockchain"

using namespace bc::chain;
using namespace bc::database;
using namespace std::placeholders;

block_chain::block_chain(threadpool& pool,
    const blockchain::settings& chain_settings,
    const database::settings& database_settings)
  : stopped_(true),
    settings_(chain_settings),
    organizer_(pool, *this, chain_settings),
    ////read_dispatch_(pool, NAME),
    ////write_dispatch_(pool, NAME),
    transaction_pool_(pool, *this, chain_settings),
    database_(database_settings)
{
}

// Close does not call stop because there is no way to detect thread join.
block_chain::~block_chain()
{
    close();
}

// Utilities.
// ----------------------------------------------------------------------------

static hash_list to_hashes(const block_result& result)
{
    const auto count = result.transaction_count();
    hash_list hashes;
    hashes.reserve(count);

    for (size_t index = 0; index < count; ++index)
        hashes.push_back(result.transaction_hash(index));

    return hashes;
}

// Properties.
// ----------------------------------------------------------------------------

transaction_pool& block_chain::pool()
{
    return transaction_pool_;
}

const settings& block_chain::chain_settings() const
{
    return settings_;
}

// Startup and shutdown.
// ----------------------------------------------------------------------------

// Start is required and the blockchain is restartable.
bool block_chain::start()
{
    if (!stopped() || !database_.start())
        return false;

    stopped_ = false;
    organizer_.start();
    transaction_pool_.start();
    return true;
}

// Stop is not required, speeds work shutdown with multiple threads.
bool block_chain::stop()
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section.
    unique_lock lock(mutex_);

    stopped_ = true;
    organizer_.stop();
    transaction_pool_.stop();
    return database_.stop();
    ///////////////////////////////////////////////////////////////////////////
}

// Database threads must be joined before close is called (or destruct).
bool block_chain::close()
{
    return database_.close();
}

// private
bool block_chain::stopped() const
{
    // TODO: consider relying on the database stopped state.
    return stopped_;
}

// Subscriber
// ------------------------------------------------------------------------

void block_chain::subscribe_reorganize(reorganize_handler handler)
{
    // Pass this through to the organizer, which issues the notifications.
    organizer_.subscribe_reorganize(handler);
}

// simple_chain (no locks, not thread safe).
// ----------------------------------------------------------------------------

bool block_chain::get_gap_range(uint64_t& out_first,
    uint64_t& out_last) const
{
    size_t first;
    size_t last;

    if (!database_.blocks.gap_range(first, last))
        return false;

    out_first = static_cast<uint64_t>(first);
    out_last = static_cast<uint64_t>(last);
    return true;
}

bool block_chain::get_next_gap(uint64_t& out_height,
    uint64_t start_height) const
{
    if (stopped())
        return false;

    BITCOIN_ASSERT(start_height <= max_size_t);
    const auto start = static_cast<size_t>(start_height);
    size_t out;

    if (!database_.blocks.next_gap(out, start))
        return false;

    out_height = static_cast<uint64_t>(out);
    return true;
}

bool block_chain::get_difficulty(hash_number& out_difficulty,
    uint64_t height) const
{
    size_t top;
    if (!database_.blocks.top(top))
        return false;

    out_difficulty = 0;
    for (uint64_t index = height; index <= top; ++index)
    {
        const auto bits = database_.blocks.get(index).header().bits;
        out_difficulty += bc::chain::block::work(bits);
    }

    return true;
}

bool block_chain::get_header(header& out_header, uint64_t height) const
{
    auto result = database_.blocks.get(height);
    if (!result)
        return false;

    out_header = std::move(result.header());
    return true;
}

bool block_chain::get_height(uint64_t& out_height,
    const hash_digest& block_hash) const
{
    auto result = database_.blocks.get(block_hash);
    if (!result)
        return false;

    out_height = result.height();
    return true;
}

bool block_chain::get_last_height(uint64_t& out_height) const
{
    size_t top;
    if (database_.blocks.top(top))
    {
        out_height = static_cast<uint64_t>(top);
        return true;
    }

    return false;
}

bool block_chain::get_outpoint_transaction(hash_digest& out_hash,
    const output_point& outpoint) const
{
    const auto spend = database_.spends.get(outpoint);
    if (!spend.valid)
        return false;

    out_hash = std::move(spend.hash);
    return true;
}

transaction_ptr block_chain::get_transaction(uint64_t& out_block_height,
    const hash_digest& transaction_hash) const
{
    const auto result = database_.transactions.get(transaction_hash);
    if (!result)
        return nullptr;

    out_block_height = result.height();
    return std::make_shared<message::transaction_message>(
        result.transaction());
}

bool block_chain::get_transaction_height(uint64_t& out_block_height,
    const hash_digest& transaction_hash) const
{
    const auto result = database_.transactions.get(transaction_hash);
    if (!result)
        return false;

    out_block_height = result.height();
    return true;
}

// This is safe to call concurrently (but with no other methods).
bool block_chain::import(block_const_ptr block, uint64_t height)
{
    if (stopped())
        return false;

    // THIS IS THE DATABASE BLOCK WRITE AND INDEX OPERATION.
    database_.push(*block, height);
    return true;
}

bool block_chain::push(block_const_ptr block)
{
    database_.push(*block);
    return true;
}

bool block_chain::pop_from(block_const_ptr_list& out_blocks,
    uint64_t height)
{
    size_t top;

    // The chain has no genesis block, fail.
    if (!database_.blocks.top(top))
        return false;

    BITCOIN_ASSERT_MSG(top <= max_size_t - 1, "chain overflow");

    // The fork is at the top of the chain, nothing to pop.
    if (height == top + 1)
        return true;

    // The fork is disconnected from the chain, fail.
    if (height > top)
        return false;

    // If the fork is at the top there is one block to pop, and so on.
    out_blocks.reserve(top - height + 1);

    for (uint64_t index = top; index >= height; --index)
        out_blocks.push_back(std::make_shared<message::block_message>(
            database_.pop()));

    return true;
}

// full_chain (internal locks).
// ----------------------------------------------------------------------------

void block_chain::start_write()
{
    DEBUG_ONLY(const auto result =) database_.begin_write();
    BITCOIN_ASSERT(result);
}

// This call is sequential, but we are preserving the callback model for now.
void block_chain::store(block_const_ptr block,
    block_store_handler handler)
{
    // We moved write to the network thread using a critical section here.
    // We do not want to give the thread to any other activity at this point.
    // A flood of valid orphans from multiple peers could tie up the CPU here,
    // but resolving that cleanly requires removing the orphan pool.

    ////write_dispatch_.ordered(
    ////    std::bind(&block_chain::do_store,
    ////        this, block, handler));

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section.
    mutex_.lock();

    const auto stopped = stopped_.load();

    if (!stopped)
        do_store(block, handler);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (stopped)
        handler(error::service_stopped, 0);
}

// This processes the block through the organizer.
void block_chain::do_store(block_const_ptr block,
    block_store_handler handler)
{
    code ec;
    start_write();

    if (database_.blocks.get(block->hash()))
    {
        stop_write(handler, error::duplicate, 0);
        return;
    }

    if ((ec = organizer_.reorganize(block)))
    {
        stop_write(handler, ec, 0);
        return;
    }

    // Get the particular block's status.
    stop_write(handler, block->metadata.validation_result,
        block->metadata.validation_height);
}

// This performs a query in the context of the calling thread.
// This allows channels to run concurrently with internal order preservation.
// The callback model is preserved currently in order to limit downstream changes.
void block_chain::fetch_serial(perform_read_functor perform_read) const
{
    // Post IBD writes are ordered on the strand, so never concurrent.
    // Reads are unordered and concurrent, but effectively blocked by writes.
    const auto try_read = [this, perform_read]()
    {
        const auto handle = database_.begin_read();
        return (!database_.is_write_locked(handle) && perform_read(handle));
    };

    const auto do_read = [this, try_read]()
    {
        // Sleep while waiting for write to complete.
        while (!try_read())
            std::this_thread::sleep_for(asio::milliseconds(10));
    };

    // Initiate serial read operation.
    do_read();
}

// full_chain (formerly fetch_ordered)
// ----------------------------------------------------------------------------

// This may generally execute 29+ queries.
// TODO: collect asynchronous calls in a function invoked directly by caller.
void block_chain::fetch_block_locator(
    block_locator_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, handler](size_t slock)
    {
        size_t top;
        code ec(error::operation_failed);

        if (!database_.blocks.top(top))
            return finish_fetch(slock, handler, ec, hash_list{});

        ec = error::success;
        const auto heights = block::locator_heights(top);

        hash_list locator;
        locator.reserve(heights.size());

        for (const auto height: heights)
        {
            const auto result = database_.blocks.get(height);

            if (!result)
            {
                ec = error::not_found;
                break;
            }

            locator.push_back(result.header().hash());
        }

        return finish_fetch(slock, handler, ec, std::move(locator));
    };
    fetch_serial(do_fetch);
}

// This may execute over 500 queries.
void block_chain::fetch_locator_block_hashes(get_blocks_const_ptr locator,
    const hash_digest& threshold, size_t limit,
    locator_block_hashes_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.
    const auto do_fetch = [this, locator, threshold, limit, handler](
        size_t slock)
    {
        // Find the first block height.
        // If no start block is on our chain we start with block 0.
        size_t start = 0;
        for (const auto& hash: locator->start_hashes)
        {
            const auto result = database_.blocks.get(hash);
            if (result)
            {
                start = result.height();
                break;
            }
        }

        // Find the stop block height.
        // The maximum stop block is 501 blocks after start (to return 500).
        size_t stop = start + limit + 1;
        if (locator->stop_hash != null_hash)
        {
            // If the stop block is not on chain we treat it as a null stop.
            const auto stop_result = database_.blocks.get(locator->stop_hash);
            if (stop_result)
                stop = std::min(stop_result.height(), stop);
        }

        // Find the threshold block height.
        // If the threshold is above the start it becomes the new start.
        if (threshold != null_hash)
        {
            const auto start_result = database_.blocks.get(threshold);
            if (start_result)
                start = std::max(start_result.height(), start);
        }

        ////////////////////////// TODO: parallelize. /////////////////////////
        // Build the hash list until we hit last or the blockchain top.
        hash_list hashes;
        for (size_t index = start + 1; index < stop; ++index)
        {
            const auto result = database_.blocks.get(index);
            if (result)
            {
                hashes.push_back(result.header().hash());
                break;
            }
        }
        ///////////////////////////////////////////////////////////////////////

        return finish_fetch(slock, handler, error::success, hashes);
    };
    fetch_serial(do_fetch);
}

// This may execute over 2000 queries.
void block_chain::fetch_locator_block_headers(
    get_headers_const_ptr locator, const hash_digest& threshold, size_t limit,
    locator_block_headers_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.
    const auto do_fetch = [this, locator, threshold, limit, handler](
        size_t slock)
    {
        // TODO: consolidate this portion with fetch_locator_block_hashes.
        //---------------------------------------------------------------------
        // Find the first block height.
        // If no start block is on our chain we start with block 0.
        size_t start = 0;
        for (const auto& hash: locator->start_hashes)
        {
            const auto result = database_.blocks.get(hash);
            if (result)
            {
                start = result.height();
                break;
            }
        }

        // Find the stop block height.
        // The maximum stop block is 501 blocks after start (to return 500).
        size_t stop = start + limit + 1;
        if (locator->stop_hash != null_hash)
        {
            // If the stop block is not on chain we treat it as a null stop.
            const auto stop_result = database_.blocks.get(locator->stop_hash);
            if (stop_result)
                stop = std::min(stop_result.height(), stop);
        }

        // Find the threshold block height.
        // If the threshold is above the start it becomes the new start.
        if (threshold != null_hash)
        {
            const auto start_result = database_.blocks.get(threshold);
            if (start_result)
                start = std::max(start_result.height(), start);
        }
        //---------------------------------------------------------------------

        ////////////////////////// TODO: parallelize. /////////////////////////
        // Build the hash list until we hit last or the blockchain top.
        chain::header::list headers;
        for (size_t index = start + 1; index < stop; ++index)
        {
            const auto result = database_.blocks.get(index);
            if (result)
            {
                headers.push_back(result.header());
                break;
            }
        }
        ///////////////////////////////////////////////////////////////////////

        return finish_fetch(slock, handler, error::success, headers);
    };
    fetch_serial(do_fetch);
}

// This may execute up to 500 queries.
void block_chain::filter_blocks(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    const auto do_fetch = [this, message, handler](size_t slock)
    {
        auto& inventories = message->inventories;
        const auto& blocks = database_.blocks;

        for (auto it = inventories.begin(); it != inventories.end();)
            if (it->is_block_type() && blocks.get(it->hash))
                it = inventories.erase(it);
            else
                ++it;

        return finish_fetch(slock, handler, error::success);
    };
    fetch_serial(do_fetch);
}

// BUGBUG: should only remove unspent transactions, other dups ok (BIP30).
void block_chain::filter_transactions(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    const auto do_fetch = [this, message, handler](size_t slock)
    {
        auto& inventories = message->inventories;
        const auto& transactions = database_.transactions;

        for (auto it = inventories.begin(); it != inventories.end();)
        {
            if (it->is_transaction_type() && transactions.get(it->hash))
                it = inventories.erase(it);
            else
                ++it;
        }

        return finish_fetch(slock, handler, error::success);
    };
    fetch_serial(do_fetch);
}

/// filter out block hashes that exist in the orphan pool.
void block_chain::filter_orphans(get_data_ptr message,
    result_handler handler) const
{
    organizer_.filter_orphans(message);
    handler(error::success);
}

// full_chain (formerly fetch_parallel)
// ------------------------------------------------------------------------

void block_chain::fetch_block(uint64_t height,
    block_fetch_handler handler) const
{
    // This is big so it is implemented in a helper class.
    blockchain::fetch_block(*this, height, handler);
}

void block_chain::fetch_block(const hash_digest& hash,
    block_fetch_handler handler) const
{
    // This is big so it is implemented in a helper class.
    blockchain::fetch_block(*this, hash, handler);
}

void block_chain::fetch_block_header(uint64_t height,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [this, height, handler](size_t slock)
    {
        const auto result = database_.blocks.get(height);

        if (!result)
            return finish_fetch(slock, handler, error::not_found, nullptr, 0);

        const auto header = std::make_shared<message::header_message>(
            result.header());

        // Asign the optional tx count to the header.
        header->transaction_count = result.transaction_count();
        return finish_fetch(slock, handler, error::success, header,
            result.height());
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);

        if (!result)
            return finish_fetch(slock, handler, error::not_found, nullptr, 0);

        const auto header = std::make_shared<message::header_message>(
            result.header());

        // Asign the optional tx count to the header.
        header->transaction_count = result.transaction_count();
        return finish_fetch(slock, handler, error::success, header,
            result.height());
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_block_transaction_hashes(uint64_t height,
    transaction_hashes_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, height, handler](size_t slock)
    {
        const auto result = database_.blocks.get(height);
        return result ?
            finish_fetch(slock, handler, error::success, to_hashes(result)) :
            finish_fetch(slock, handler, error::not_found, hash_list{});
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_block_transaction_hashes(const hash_digest& hash,
    transaction_hashes_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, to_hashes(result)) :
            finish_fetch(slock, handler, error::not_found, hash_list{});
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_block_height(const hash_digest& hash,
    block_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.height()) :
            finish_fetch(slock, handler, error::not_found, 0);
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_last_height(
    last_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, handler](size_t slock)
    {
        size_t last_height;
        return database_.blocks.top(last_height) ?
            finish_fetch(slock, handler, error::success, last_height) :
            finish_fetch(slock, handler, error::not_found, 0);
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_transaction(const hash_digest& hash,
    transaction_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.transactions.get(hash);

        if (!result)
            return finish_fetch(slock, handler, error::not_found, nullptr, 0);

        const auto tx = std::make_shared<message::transaction_message>(
            result.transaction());

        return finish_fetch(slock, handler, error::success, tx,
            result.height());
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_transaction_index(const hash_digest& hash,
    transaction_index_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {}, {});
        return;
    }

    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.transactions.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.height(),
                result.index()) :
            finish_fetch(slock, handler, error::not_found, 0, 0);
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, outpoint, handler](size_t slock)
    {
        const auto spend = database_.spends.get(outpoint);
        return spend.valid ?
            finish_fetch(slock, handler, error::success,
                chain::input_point{ std::move(spend.hash), spend.index }) :
            finish_fetch(slock, handler, error::not_found,
                chain::input_point{});
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_history(const wallet::payment_address& address,
    uint64_t limit, uint64_t from_height, history_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, address, handler, limit, from_height](
        size_t slock)
    {
        return finish_fetch(slock, handler, error::success,
            database_.history.get(address.hash(), limit, from_height));
    };
    fetch_serial(do_fetch);
}

void block_chain::fetch_stealth(const binary& filter, uint64_t from_height,
    stealth_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [this, filter, handler, from_height](size_t slock)
    {
        return finish_fetch(slock, handler, error::success,
            database_.stealth.scan(filter, from_height));
    };
    fetch_serial(do_fetch);
}

} // namespace blockchain
} // namespace libbitcoin
