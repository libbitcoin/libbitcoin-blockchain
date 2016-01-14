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
#include <bitcoin/blockchain/implementation/blockchain_impl.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/implementation/organizer_impl.hpp>
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

#define NAME "blockchain"
#define BC_CHAIN_DATABASE_LOCK_FILE "db-lock"
    
using namespace bc::message;
using namespace boost::interprocess;
using path = boost::filesystem::path;

// This is a protocol limit that we incorporate into the query.
static constexpr size_t maximum_get_blocks = 500;

static file_lock init_lock(const path& prefix)
{
    // Touch the lock file (open/close).
    const auto lockfile = prefix / BC_CHAIN_DATABASE_LOCK_FILE;
    bc::ofstream file(lockfile.string(), std::ios::app);
    file.close();
    return file_lock(lockfile.string().c_str());
}

// TODO: move threadpool management into the implementation (see network::p2p).
blockchain_impl::blockchain_impl(threadpool& pool, const settings& settings)
  : read_dispatch_(pool, NAME),
    write_dispatch_(pool, NAME),
    flock_(init_lock(settings.database_path)),
    slock_(0),
    stopped_(true),
    store_(settings.database_path),
    database_(store_, settings.history_start_height),
    orphans_(settings.block_pool_capacity),
    chain_(database_),
    organizer_(pool, database_, orphans_, chain_, settings.use_testnet_rules,
        settings.checkpoints)
{
}

void blockchain_impl::start(result_handler handler)
{
    if (!flock_.try_lock())
    {
        handler(error::operation_failed);
        return;
    }

    // TODO: can we actually restart?
    stopped_ = false;

    database_.start();
    handler(error::success);
}

void blockchain_impl::stop()
{
    const auto unhandled = [](const code){};
    stop(unhandled);
}

// TODO: close all file descriptors once threadpool has stopped and return
// result of file close in handler.
void blockchain_impl::stop(result_handler handler)
{
    stopped_ = true;
    organizer_.stop();
    handler(error::success);
}

bool blockchain_impl::stopped()
{
    return stopped_;
}

void blockchain_impl::subscribe_reorganize(reorganize_handler handler)
{
    // Pass this through to the organizer, which issues the notifications.
    organizer_.subscribe_reorganize(handler);
}

void blockchain_impl::start_write()
{
    DEBUG_ONLY(const auto lock =) ++slock_;

    // slock was now odd.
    BITCOIN_ASSERT(lock % 2 == 1);
}

void blockchain_impl::store(std::shared_ptr<chain::block> block,
    block_store_handler handler)
{
    write_dispatch_.ordered(
        std::bind(&blockchain_impl::do_store,
            this, block, handler));
}

void blockchain_impl::do_store(std::shared_ptr<chain::block> block,
    block_store_handler handler)
{
    if (stopped())
        return;

    start_write();

    uint64_t height;
    if (chain_.find_height(height, block->header.hash()))
    {
        const auto info = block_info{ block_status::confirmed, height };
        stop_write(handler, error::duplicate, info);
        return;
    }

    const auto detail = std::make_shared<block_detail>(block);
    if (!orphans_.add(detail))
    {
        static constexpr uint64_t height = 0;
        const auto info = block_info{ block_status::orphan, height };
        stop_write(handler, error::duplicate, info);
        return;
    }

    organizer_.start();
    stop_write(handler, detail->error(), detail->info());
}

void blockchain_impl::import(std::shared_ptr<chain::block> block,
    block_import_handler handler)
{
    const auto do_import = [this, block, handler]()
    {
        start_write();
        database_.push(*block);
        stop_write(handler, error::success);
    };
    write_dispatch_.ordered(do_import);
}

void blockchain_impl::fetch_parallel(perform_read_functor perform_read)
{
    // Writes are ordered on the strand, so never concurrent.
    // Reads are unordered and concurrent, but effectively blocked by writes.
    const auto try_read = [this, perform_read]() -> bool
    {
        // Implements the seqlock counter logic.
        const size_t slock = slock_.load();
        return ((slock % 2 == 0) && perform_read(slock));
    };

    // The read is invalidated only if a write overlapped the read.
    // This is less efficient than waiting on a write lock, since it requires
    // a large arbitrary quanta and produces disacarded reads. It also
    // complicates implementation in that read during write must be safe. This
    // lack of safety produced the longstanding appearance-of-corruption during
    // sync bug (which was resolved by making the read safe during write).
    // If reads could be made *valid* during write this sleep could be removed
    // and the design would be dramtically *more* efficient than even a read
    // lock during write.
    const auto do_read = [this, try_read]()
    {
        // Sleeping while waiting for write to complete.
        while (!try_read())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    // Initiate async read operation.
    read_dispatch_.concurrent(do_read);
}

void blockchain_impl::fetch_ordered(perform_read_functor perform_read)
{
    const auto try_read = [this, perform_read]() -> bool
    {
        // Implements the seqlock counter logic.
        const size_t slock = slock_.load();
        return ((slock % 2 != 1) && perform_read(slock));
    };

    const auto do_read = [this, try_read]()
    {
        // Sleeping while waiting for write to complete.
        while (!try_read())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    // Initiate async read operation.
    read_dispatch_.ordered(do_read);
}

// This may generally execute 29+ queries.
// TODO: Collect asynchronous calls in a function invoked directly by caller.
void blockchain_impl::fetch_block_locator(block_locator_fetch_handler handler)
{
    const auto do_fetch = [this, handler](size_t slock)
    {
        get_blocks locator;
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

            locator.start_hashes.push_back(result.header().hash());
        }

        return finish_fetch(slock, handler,
            error::success, locator);
    };

    fetch_ordered(do_fetch);
}

static hash_list to_hashes(const block_result& result)
{
    hash_list hashes;
    for (size_t index = 0; index < result.transactions_size(); ++index)
        hashes.push_back(result.transaction_hash(index));

    return hashes;
}

// Fetch start-base-stop|top+1(max 500)
// This may generally execute 502 but as many as 531+ queries.
void blockchain_impl::fetch_locator_block_hashes(const get_blocks& locator,
    const hash_digest& threshold,
    locator_block_hashes_fetch_handler handler)
{
    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.
    const auto do_fetch = [this, locator, threshold, handler](
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
        size_t stop = start + maximum_get_blocks + 1;
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

// This may generally execute up to 500 queries.
void blockchain_impl::fetch_missing_block_hashes(const hash_list& hashes,
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

void blockchain_impl::fetch_block_header(uint64_t height,
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

void blockchain_impl::fetch_block_header(const hash_digest& hash,
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

void blockchain_impl::fetch_block_transaction_hashes(
    const hash_digest& hash, transaction_hashes_fetch_handler handler)
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

void blockchain_impl::fetch_block_height(const hash_digest& hash,
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

void blockchain_impl::fetch_last_height(last_height_fetch_handler handler)
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

void blockchain_impl::fetch_transaction(const hash_digest& hash,
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

void blockchain_impl::fetch_transaction_index(const hash_digest& hash,
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

void blockchain_impl::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler)
{
    const auto do_fetch = [this, outpoint, handler](size_t slock)
    {
        const auto result = database_.spends.get(outpoint);
        const auto point = result ?
            chain::input_point{ result.hash(), result.index() } :
            chain::input_point();
        return result ?
            finish_fetch(slock, handler, error::success, point) :
            finish_fetch(slock, handler, error::unspent_output, point);
    };
    fetch_parallel(do_fetch);
}

void blockchain_impl::fetch_history(const wallet::payment_address& address,
    history_fetch_handler handler, const uint64_t limit,
    const uint64_t from_height)
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

void blockchain_impl::fetch_stealth(const binary_type& filter,
    stealth_fetch_handler handler, uint64_t from_height)
{
    const auto do_fetch = [this, filter, handler, from_height](
        size_t slock)
    {
        const auto stealth = database_.stealth.scan(filter, from_height);
        return finish_fetch(slock, handler, error::success, stealth);
    };
    fetch_parallel(do_fetch);
}

} // namespace blockchain
} // namespace libbitcoin
