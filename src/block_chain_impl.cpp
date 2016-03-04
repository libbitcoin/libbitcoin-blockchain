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
#include <bitcoin/blockchain/block_chain_impl.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

#define NAME "blockchain"

using namespace bc::chain;
using namespace bc::database;
using namespace boost::interprocess;
using boost::filesystem::path;

// TODO: move threadpool management into the implementation (see network::p2p).
block_chain_impl::block_chain_impl(threadpool& pool,
    database::data_base& database, const settings& settings)
  : stopped_(true),
    organizer_(pool, *this, settings),
    read_dispatch_(pool, NAME),
    write_dispatch_(pool, NAME),
    settings_(settings),
    database_(database)
{
}

// ----------------------------------------------------------------------------
// Utilities.

static hash_list to_hashes(const block_result& result)
{
    hash_list hashes;
    for (size_t index = 0; index < result.transaction_count(); ++index)
        hashes.push_back(result.transaction_hash(index));

    return hashes;
}

// ----------------------------------------------------------------------------
// Properties.

const settings& block_chain_impl::chain_settings() const
{
    return settings_;
}

bool block_chain_impl::stopped()
{
    // TODO: consider relying on a database stopped state.
    return stopped_;
}

// ----------------------------------------------------------------------------
// Start sequence.

void block_chain_impl::start(result_handler handler)
{
    if (!database_.start())
    {
        handler(error::operation_failed);
        return;
    }

    stopped_ = false;
    handler(error::success);
}

// ----------------------------------------------------------------------------
// Stop sequence.

void block_chain_impl::stop()
{
    const auto unhandled = [](const code){};
    stop(unhandled);
}

void block_chain_impl::stop(result_handler handler)
{
    stopped_ = true;
    organizer_.stop();
    handler(database_.stop() ? error::success : error::file_system);
}

// ----------------------------------------------------------------------------
// simple_chain (no locks).

bool block_chain_impl::get_difficulty(hash_number& out_difficulty,
    uint64_t height)
{
    size_t top;
    if (!database_.blocks.top(top))
        return false;

    out_difficulty = 0;
    for (uint64_t index = height; index <= top; ++index)
    {
        const auto bits = database_.blocks.get(index).header().bits;
        out_difficulty += block_work(bits);
    }

    return true;
}

bool block_chain_impl::get_header(header& out_header, uint64_t height)
{
    auto result = database_.blocks.get(height);
    if (!result)
        return false;

    out_header = result.header();
    return true;
}

bool block_chain_impl::get_height(uint64_t& out_height,
    const hash_digest& block_hash)
{
    auto result = database_.blocks.get(block_hash);
    if (!result)
        return false;

    out_height = result.height();
    return true;
}

bool block_chain_impl::get_outpoint_transaction(hash_digest& out_transaction,
    const output_point& outpoint)
{
    const auto spend = database_.spends.get(outpoint);
    if (!spend.valid)
        return false;

    out_transaction = spend.hash;
    return true;
}

bool block_chain_impl::get_transaction(transaction& out_transaction,
    uint64_t& out_block_height, const hash_digest& transaction_hash)
{
    const auto result = database_.transactions.get(transaction_hash);
    if (!result)
        return false;

    out_transaction = result.transaction();
    out_block_height = result.height();
    return true;
}

bool block_chain_impl::push(block_detail::ptr block)
{
    database_.push(block->actual());
    return true;
}

bool block_chain_impl::pop_from(block_detail::list& out_blocks,
    uint64_t height)
{
    size_t top;
    if (!database_.blocks.top(top))
        return false;

    for (uint64_t index = top; index >= height; --index)
    {
        const auto block = std::make_shared<block_detail>(database_.pop());
        out_blocks.push_back(block);
    }

    return true;
}

// ----------------------------------------------------------------------------
// block_chain (internal locks).

// This is not deconflicted with the blockchain dispatch queue, do not import
// concurrently with store or query operations.
bool block_chain_impl::import(block::ptr block, uint64_t height)
{
    if (stopped())
        return false;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    std::lock_guard<std::mutex> lock(mutex_);

    // THIS IS THE DATABASE BLOCK WRITE AND INDEX OPERATION.
    database_.push(*block, height);

    return true;
    ///////////////////////////////////////////////////////////////////////////
}

void block_chain_impl::start_write()
{
    DEBUG_ONLY(const auto result =) database_.begin_write();
    BITCOIN_ASSERT(result);
}

void block_chain_impl::store(block::ptr block, block_store_handler handler)
{
    if (stopped())
        return;

    write_dispatch_.ordered(
        std::bind(&block_chain_impl::do_store,
            this, block, handler));
}

// This orders the block via the organizer.
void block_chain_impl::do_store(block::ptr block, block_store_handler handler)
{
    if (stopped())
        return;

    start_write();

    // Disallow duplicate of on-chain block.
    auto result = database_.blocks.get(block->header.hash());
    if (result)
    {
        const auto height = result.height();
        const auto info = block_info{ block_status::confirmed, height };
        stop_write(handler, error::duplicate, info);
        return;
    }

    // Disallow duplicate of in-pool block.
    const auto detail = std::make_shared<block_detail>(block);
    if (!organizer_.add(detail))
    {
        const auto info = block_info{ block_status::orphan, 0 };
        stop_write(handler, error::duplicate, info);
        return;
    }

    // See replace_chain for block push.
    organizer_.organize();
    stop_write(handler, detail->error(), detail->info());
}

void block_chain_impl::fetch_parallel(perform_read_functor perform_read)
{
    if (stopped())
        return;

    // Writes are ordered on the strand, so never concurrent.
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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    // Initiate async read operation.
    read_dispatch_.concurrent(do_read);
}

///////////////////////////////////////////////////////////////////////////////
// TODO: This should be ordered on the channel's strand, not across channels.
///////////////////////////////////////////////////////////////////////////////
void block_chain_impl::fetch_ordered(perform_read_functor perform_read)
{
    if (stopped())
        return;

    // Writes are ordered on the strand, so never concurrent.
    // Reads are unordered and concurrent, but effectively blocked by writes.
    const auto try_read = [this, perform_read]() -> bool
    {
        const auto handle = database_.begin_read();
        return (!database_.is_write_locked(handle) && perform_read(handle));
    };

    const auto do_read = [this, try_read]()
    {
        // Sleep while waiting for write to complete.
        while (!try_read())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    // Initiate async read operation.
    read_dispatch_.ordered(do_read);
}

// This may generally execute 29+ queries.
// TODO: Collect asynchronous calls in a function invoked directly by caller.
void block_chain_impl::fetch_block_locator(block_locator_fetch_handler handler)
{
    const auto do_fetch = [this, handler](size_t slock)
    {
        message::block_locator locator;
        size_t top_height;
        if (!database_.blocks.top(top_height))
            return finish_fetch(slock, handler,
                error::operation_failed, locator);

        const auto indexes = block_locator_indexes(top_height);
        for (auto index = indexes.begin(); index != indexes.end(); ++index)
        {
            const auto result = database_.blocks.get(*index);
            if (!result)
                return finish_fetch(slock, handler,
                    error::not_found, locator);

            locator.push_back(result.header().hash());
        }

        return finish_fetch(slock, handler,
            error::success, locator);
    };

    fetch_ordered(do_fetch);
}

// Fetch start-base-stop|top+1(max 500)
// This may generally execute 502 but as many as 531+ queries.
void block_chain_impl::fetch_locator_block_hashes(
    const message::get_blocks& locator, const hash_digest& threshold,
    size_t limit, locator_block_hashes_fetch_handler handler)
{
    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.
    const auto do_fetch = [this, locator, threshold, limit, handler](
        size_t slock)
    {
        // Find the first block height.
        // If no start block is on our chain we start with block 0.
        size_t start = 0;
        for (const auto& hash: locator.start_hashes)
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
        if (locator.stop_hash != null_hash)
        {
            // If the stop block is not on chain we treat it as a null stop.
            const auto stop_result = database_.blocks.get(locator.stop_hash);
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

        // This largest portion can be parallelized.
        // Build the hash list until we hit last or the blockchain top.
        hash_list hashes;
        for (size_t index = start + 1; index < stop; ++index)
        {
            const auto result = database_.blocks.get(index);
            if (!result)
            {
                hashes.push_back(result.header().hash());
                break;
            }
        }

        return finish_fetch(slock, handler,
            error::success, hashes);
    };
    fetch_ordered(do_fetch);
}

// This may execute up to 500 queries.
void block_chain_impl::fetch_missing_block_hashes(const hash_list& hashes,
    missing_block_hashes_fetch_handler handler)
{
    const auto do_fetch = [this, hashes, handler](size_t slock)
    {
        hash_list missing;
        for (const auto& hash: hashes)
            if (!database_.blocks.get(hash))
                missing.push_back(hash);

        return finish_fetch(slock, handler, error::success, missing);
    };
    fetch_ordered(do_fetch);
}

void block_chain_impl::fetch_block_header(uint64_t height,
    block_header_fetch_handler handler)
{
    const auto do_fetch = [this, height, handler](size_t slock)
    {
        const auto result = database_.blocks.get(height);
        return result ?
            finish_fetch(slock, handler, error::success, result.header()) :
            finish_fetch(slock, handler, error::not_found, chain::header());
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.header()) :
            finish_fetch(slock, handler, error::not_found, chain::header());
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_block_transaction_hashes(uint64_t height,
    transaction_hashes_fetch_handler handler)
{
    const auto do_fetch = [this, height, handler](size_t slock)
    {
        const auto result = database_.blocks.get(height);
        return result ?
            finish_fetch(slock, handler, error::success, to_hashes(result)) :
            finish_fetch(slock, handler, error::not_found, hash_list());
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_block_transaction_hashes(const hash_digest& hash,
    transaction_hashes_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, to_hashes(result)) :
            finish_fetch(slock, handler, error::not_found, hash_list());
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_block_height(const hash_digest& hash,
    block_height_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.height()) :
            finish_fetch(slock, handler, error::not_found, 0);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_last_height(last_height_fetch_handler handler)
{
    const auto do_fetch = [this, handler](size_t slock)
    {
        size_t last_height;
        return database_.blocks.top(last_height) ?
            finish_fetch(slock, handler, error::success, last_height) :
            finish_fetch(slock, handler, error::not_found, 0);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_transaction(const hash_digest& hash,
    transaction_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.transactions.get(hash);
        const auto tx = result ? result.transaction() : chain::transaction();
        return result ?
            finish_fetch(slock, handler, error::success, tx) :
            finish_fetch(slock, handler, error::not_found, tx);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_transaction_index(const hash_digest& hash,
    transaction_index_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.transactions.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.height(),
                result.index()) :
            finish_fetch(slock, handler, error::not_found, 0, 0);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler)
{
    const auto do_fetch = [this, outpoint, handler](size_t slock)
    {
        const auto spend = database_.spends.get(outpoint);
        const auto point = spend.valid ?
            chain::input_point{ spend.hash, spend.index } :
            chain::input_point();
        return spend.valid ?
            finish_fetch(slock, handler, error::success, point) :
            finish_fetch(slock, handler, error::unspent_output, point);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_history(const wallet::payment_address& address,
    uint64_t limit, uint64_t from_height, history_fetch_handler handler)
{
    const auto do_fetch = [this, address, handler, limit, from_height](
        size_t slock)
    {
        const auto history = database_.history.get(address.hash(), limit,
            from_height);
        return finish_fetch(slock, handler, error::success, history);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::fetch_stealth(const binary& filter, uint64_t from_height,
    stealth_fetch_handler handler)
{
    const auto do_fetch = [this, filter, handler, from_height](
        size_t slock)
    {
        const auto stealth = database_.stealth.scan(filter, from_height);
        return finish_fetch(slock, handler, error::success, stealth);
    };
    fetch_parallel(do_fetch);
}

void block_chain_impl::subscribe_reorganize(reorganize_handler handler)
{
    // Pass this through to the organizer, which issues the notifications.
    organizer_.subscribe_reorganize(handler);
}

} // namespace blockchain
} // namespace libbitcoin
