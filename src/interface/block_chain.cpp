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
#include <bitcoin/blockchain/interface/block_chain.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/interface/block_fetcher.hpp>
#include <bitcoin/blockchain/pools/orphan_pool.hpp>
#include <bitcoin/blockchain/pools/orphan_pool_manager.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::database;
using namespace bc::message;
using namespace bc::database;
using namespace std::placeholders;

block_chain::block_chain(threadpool& pool,
    const blockchain::settings& chain_settings,
    const database::settings& database_settings)
  : stopped_(true),
    settings_(chain_settings),
    spin_lock_sleep_(asio::milliseconds(1)),
    orphan_pool_(chain_settings.block_pool_capacity),
    orphan_manager_(pool, *this, orphan_pool_, chain_settings),
    transaction_pool_(pool, *this, chain_settings),
    database_(database_settings)
{
}

// ============================================================================
// FAST CHAIN
// ============================================================================

// Readers.
// ----------------------------------------------------------------------------

bool block_chain::get_gaps(block_database::heights& out_gaps) const
{
    database_.blocks().gaps(out_gaps);
    return true;
}

bool block_chain::get_block_exists(const hash_digest& block_hash) const
{
    return database_.blocks().get(block_hash);
}

bool block_chain::get_block_hash(hash_digest& out_hash, size_t height) const
{
    const auto result = database_.blocks().get(height);

    if (!result)
        return false;

    out_hash = result.hash();
    return true;
}

bool block_chain::get_fork_difficulty(uint256_t& out_difficulty,
    const uint256_t& maximum, size_t from_height) const
{
    size_t top;
    if (!database_.blocks().top(top))
        return false;

    out_difficulty = 0;
    for (auto height = from_height; height <= top && out_difficulty < maximum;
        ++height)
    {
        const auto result = database_.blocks().get(height);
        if (!result)
            return false;

        out_difficulty += chain::block::difficulty(result.bits());
    }

    return true;
}

bool block_chain::get_header(chain::header& out_header, size_t height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_header = result.header();
    return true;
}

bool block_chain::get_height(size_t& out_height,
    const hash_digest& block_hash) const
{
    auto result = database_.blocks().get(block_hash);
    if (!result)
        return false;

    out_height = result.height();
    return true;
}

bool block_chain::get_bits(uint32_t& out_bits, const size_t& height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_bits = result.bits();
    return true;
}

bool block_chain::get_timestamp(uint32_t& out_timestamp,
    const size_t& height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_timestamp = result.timestamp();
    return true;
}

bool block_chain::get_version(uint32_t& out_version,
    const size_t& height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_version = result.version();
    return true;
}

bool block_chain::get_last_height(size_t& out_height) const
{
    return database_.blocks().top(out_height);
}

bool block_chain::get_output(chain::output& out_output, size_t& out_height,
    bool& out_coinbase, const chain::output_point& outpoint,
    size_t fork_height) const
{
    // This includes a cached value for spender height (or not_spent).
    // Get the highest tx with matching hash, at or below the fork height.
    return database_.transactions().get_output(out_output, out_height,
        out_coinbase, outpoint, fork_height);
}

bool block_chain::get_is_unspent_transaction(const hash_digest& hash,
    size_t fork_height) const
{
    const auto result = database_.transactions().get(hash, fork_height);
    return result && !result.is_spent(fork_height);
}

transaction_ptr block_chain::get_transaction(size_t& out_block_height,
    const hash_digest& hash) const
{
    const auto result = database_.transactions().get(hash, max_size_t);
    if (!result)
        return nullptr;

    out_block_height = result.height();
    return std::make_shared<transaction>(result.transaction());
}

// Synchronous write sequence.
// ----------------------------------------------------------------------------

bool block_chain::insert(block_const_ptr block, size_t height, bool flush)
{
    // End must be paired with read, regardless of result.
    const auto begin = database_.begin_write(flush);
    const auto write = begin && do_insert(*block, height);
    const auto end = database_.end_write(flush);
    return begin && write && end;
}

bool block_chain::do_insert(const chain::block& block, size_t height)
{
    return database_.insert(block, height) == error::success;
}

// Asynchronous write sequence.
// ------------------------------------------------------------------------ 

void block_chain::reorganize(fork::const_ptr fork,
    block_const_ptr_list_ptr outgoing_blocks, bool flush,
    dispatcher& dispatch, complete_handler handler)
{
    // The pop handler closes the write operation.
    const result_handler pop_handler =
        std::bind(&block_chain::handle_pop,
            this, _1, fork, flush, std::ref(dispatch), handler);

    if (!database_.begin_write(flush))
    {
        pop_handler(error::operation_failed);
        return;
    }

    database_.pop_above(outgoing_blocks, fork->hash(), dispatch, pop_handler);
}

void block_chain::handle_pop(const code& ec, fork::const_ptr fork, bool flush,
    dispatcher& dispatch, result_handler handler)
{
    const result_handler push_handler =
        std::bind(&block_chain::handle_push,
            this, _1, flush, handler);

    if (ec)
    {
        push_handler(ec);
        return;
    }

    auto fork_height = safe_add(fork->height(), size_t(1));
    database_.push_all(fork->blocks(), fork_height, std::ref(dispatch),
        push_handler);
}

void block_chain::handle_push(const code& ec, bool flush,
    result_handler handler)
{
    const auto end_code = database_.end_write(flush) ?
        error::success : error::operation_failed;

    handler(ec ? ec : end_code);
}

// ============================================================================
// SAFE CHAIN
// ============================================================================

// Startup and shutdown.
// ----------------------------------------------------------------------------
// TODO: make start thread safe, protect start with mutex.

bool block_chain::start()
{
    if (!stopped() || !database_.open())
        return false;

    stopped_ = false;
    return true;
}

// Pool start is deferred so that we don't overlap locks between initial block
// download and catch-up sync. The latter spans orphan pool lifetime.
bool block_chain::start_pools()
{
    transaction_pool_.start();
    return orphan_manager_.start();
}

bool block_chain::stop()
{
    stopped_ = true;
    transaction_pool_.stop();

    // This blocks until asynchronous write operations are complete, preventing
    // premature suspension of dispatch by shutdown of the threadpool.
    return orphan_manager_.stop();
}

bool block_chain::close()
{
    return database_.close();
}

block_chain::~block_chain()
{
    close();
}

// Queries.
// ----------------------------------------------------------------------------

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

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.blocks().get(height);

        if (!result)
            return finish_read(slock, handler, error::not_found, nullptr, 0);

        const auto header = std::make_shared<header_message>(result.header());

        return finish_read(slock, handler, error::success, header,
            result.height());
    };
    read_serial(do_fetch);
}

void block_chain::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.blocks().get(hash);

        if (!result)
            return finish_read(slock, handler, error::not_found, nullptr, 0);

        const auto header = std::make_shared<header_message>(result.header());

        return finish_read(slock, handler, error::success, header,
            result.height());
    };
    read_serial(do_fetch);
}

void block_chain::fetch_merkle_block(uint64_t height,
    transaction_hashes_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.blocks().get(height);

        if (!result)
            finish_read(slock, handler, error::not_found, nullptr, 0);

        auto merkle = std::make_shared<merkle_block>(
            merkle_block{ result.header(),
                safe_unsigned<uint32_t>(result.transaction_count()),
                to_hashes(result), {} });

        return finish_read(slock, handler, error::success, merkle,
            result.height());
    };
    read_serial(do_fetch);
}

void block_chain::fetch_merkle_block(const hash_digest& hash,
    transaction_hashes_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.blocks().get(hash);

        if (!result)
            finish_read(slock, handler, error::not_found, nullptr, 0);

        auto merkle = std::make_shared<merkle_block>(
            merkle_block{ result.header(),
                safe_unsigned<uint32_t>(result.transaction_count()),
                to_hashes(result), {} });
        return finish_read(slock, handler, error::success, merkle,
            result.height());
    };
    read_serial(do_fetch);
}

void block_chain::fetch_block_height(const hash_digest& hash,
    block_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.blocks().get(hash);
        return result ?
            finish_read(slock, handler, error::success, result.height()) :
            finish_read(slock, handler, error::not_found, 0);
    };
    read_serial(do_fetch);
}

void block_chain::fetch_last_height(
    last_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        size_t last_height;
        return database_.blocks().top(last_height) ?
            finish_read(slock, handler, error::success, last_height) :
            finish_read(slock, handler, error::not_found, 0);
    };
    read_serial(do_fetch);
}

void block_chain::fetch_transaction(const hash_digest& hash,
    transaction_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.transactions().get(hash, max_size_t);

        if (!result)
            return finish_read(slock, handler, error::not_found, nullptr, 0);

        const auto tx = std::make_shared<transaction>(
            result.transaction());
        return finish_read(slock, handler, error::success, tx,
            result.height());
    };
    read_serial(do_fetch);
}

void block_chain::fetch_transaction_position(const hash_digest& hash,
    transaction_index_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, 0, 0);
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.transactions().get(hash, max_size_t);
        return result ?
            finish_read(slock, handler, error::success, result.position(),
                result.height()) :
            finish_read(slock, handler, error::not_found, 0, 0);
    };
    read_serial(do_fetch);
}

void block_chain::fetch_output(const chain::output_point& outpoint,
    output_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto result = database_.transactions().get(outpoint.hash(),
            max_size_t);

        if (!result)
            return finish_read(slock, handler, error::not_found,
                chain::output{});

        const auto output = result.output(outpoint.index());
        const auto ec = output.is_valid() ? error::success : error::not_found;
        return finish_read(slock, handler, ec, std::move(output));
    };
    read_serial(do_fetch);
}

void block_chain::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        const auto point = database_.spends().get(outpoint);
        return point.hash() != null_hash ?
            finish_read(slock, handler, error::success, point) :
            finish_read(slock, handler, error::not_found, point);
    };
    read_serial(do_fetch);
}

void block_chain::fetch_history(const wallet::payment_address& address,
    uint64_t limit, uint64_t from_height, history_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        return finish_read(slock, handler, error::success,
            database_.history().get(address.hash(), limit, from_height));
    };
    read_serial(do_fetch);
}

void block_chain::fetch_stealth(const binary& filter, uint64_t from_height,
    stealth_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        return finish_read(slock, handler, error::success,
            database_.stealth().scan(filter, from_height));
    };
    read_serial(do_fetch);
}

// This may generally execute 29+ queries.
void block_chain::fetch_block_locator(const block::indexes& heights,
    block_locator_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    const auto do_fetch = [&](size_t slock)
    {
        size_t top;
        code ec(error::operation_failed);

        if (!database_.blocks().top(top))
            return finish_read(slock, handler, ec, nullptr);

        // Caller can cast down to get_blocks.
        auto get_headers = std::make_shared<message::get_headers>();
        auto& hashes = get_headers->start_hashes();
        hashes.reserve(heights.size());
        ec = error::success;

        for (const auto height: heights)
        {
            const auto result = database_.blocks().get(height);

            if (!result)
            {
                ec = error::not_found;
                hashes.clear();
                break;
            }

            hashes.push_back(result.header().hash());
        }

        hashes.shrink_to_fit();
        return finish_read(slock, handler, ec, get_headers);
    };
    read_serial(do_fetch);
}

// This may execute over 500 queries.
void block_chain::fetch_locator_block_hashes(get_blocks_const_ptr locator,
    const hash_digest& threshold, size_t limit,
    inventory_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.
    const auto do_fetch = [&](size_t slock)
    {
        // Find the first block height.
        // If no start block is on our chain we start with block 0.
        size_t start = 0;
        for (const auto& hash: locator->start_hashes())
        {
            const auto result = database_.blocks().get(hash);
            if (result)
            {
                start = result.height();
                break;
            }
        }

        // Find the stop block height.
        // The maximum stop block is 501 blocks after start (to return 500).
        auto stop = safe_add(safe_add(start, limit), size_t(1));

        if (locator->stop_hash() != null_hash)
        {
            // If the stop block is not on chain we treat it as a null stop.
            const auto stop_result = database_.blocks().get(locator->stop_hash());
            if (stop_result)
                stop = std::min(stop_result.height(), stop);
        }

        // Find the threshold block height.
        // If the threshold is above the start it becomes the new start.
        if (threshold != null_hash)
        {
            const auto start_result = database_.blocks().get(threshold);
            if (start_result)
                start = std::max(start_result.height(), start);
        }

        auto hashes = std::make_shared<inventory>();
        hashes->inventories().reserve(stop);

        // Build the hash list until we hit last or the blockchain top.
        for (size_t index = start + 1; index < stop; ++index)
        {
            const auto result = database_.blocks().get(index);
            if (result)
            {
                const auto& header = result.header();
                static const auto id = inventory::type_id::block;
                hashes->inventories().push_back({ id, header.hash() });
                break;
            }
        }

        hashes->inventories().shrink_to_fit();
        return finish_read(slock, handler, error::success, hashes);
    };
    read_serial(do_fetch);
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
    const auto do_fetch = [&](size_t slock)
    {
        // TODO: consolidate this portion with fetch_locator_block_hashes.
        //---------------------------------------------------------------------
        // Find the first block height.
        // If no start block is on our chain we start with block 0.
        size_t start = 0;
        for (const auto& hash: locator->start_hashes())
        {
            const auto result = database_.blocks().get(hash);
            if (result)
            {
                start = result.height();
                break;
            }
        }

        // Find the stop block height.
        // The maximum stop block is 501 blocks after start (to return 500).
        auto stop = safe_add(safe_add(start, limit), size_t(1));

        if (locator->stop_hash() != null_hash)
        {
            // If the stop block is not on chain we treat it as a null stop.
            const auto stop_result = database_.blocks().get(
                locator->stop_hash());

            if (stop_result)
                stop = std::min(stop_result.height(), stop);
        }

        // Find the threshold block height.
        // If the threshold is above the start it becomes the new start.
        if (threshold != null_hash)
        {
            const auto start_result = database_.blocks().get(threshold);
            if (start_result)
                start = std::max(start_result.height(), start);
        }
        //---------------------------------------------------------------------

        const auto headers = std::make_shared<message::headers>();
        headers->elements().reserve(stop);

        // Build the hash list until we hit last or the blockchain top.
        for (size_t index = start + 1; index < stop; ++index)
        {
            const auto result = database_.blocks().get(index);
            if (result)
            {
                headers->elements().push_back(result.header());
                break;
            }
        }

        headers->elements().shrink_to_fit();
        return finish_read(slock, handler, error::success, headers);
    };
    read_serial(do_fetch);
}

// Transaction Pool.
//-----------------------------------------------------------------------------

void block_chain::fetch_floaters(size_t size,
    inventory_fetch_handler handler) const
{
    transaction_pool_.fetch_inventory(size, handler);
}

// Filters.
//-----------------------------------------------------------------------------

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
        auto& inventories = message->inventories();
        const auto& blocks = database_.blocks();

        for (auto it = inventories.begin(); it != inventories.end();)
            if (it->is_block_type() && blocks.get(it->hash()))
                it = inventories.erase(it);
            else
                ++it;

        return finish_read(slock, handler, error::success);
    };
    read_serial(do_fetch);
}

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
        auto& inventories = message->inventories();
        const auto& transactions = database_.transactions();

        for (auto it = inventories.begin(); it != inventories.end();)
        {
            if (it->is_transaction_type() &&
                get_is_unspent_transaction(it->hash(), max_size_t))
                it = inventories.erase(it);
            else
                ++it;
        }

        return finish_read(slock, handler, error::success);
    };
    read_serial(do_fetch);
}

void block_chain::filter_orphans(get_data_ptr message,
    result_handler handler) const
{
    orphan_pool_.filter(message);
    handler(error::success);
}

void block_chain::filter_floaters(get_data_ptr message,
    result_handler handler) const
{
    transaction_pool_.filter(message, handler);
}

// Subscribers.
//-----------------------------------------------------------------------------

void block_chain::subscribe_reorganize(reorganize_handler&& handler)
{
    // Pass this through to the manager, which issues the notifications.
    orphan_manager_.subscribe_reorganize(std::move(handler));
}

void block_chain::subscribe_transaction(transaction_handler&& handler)
{
    // Pass this through to the tx pool, which issues the notifications.
    transaction_pool_.subscribe_transaction(std::move(handler));
}

// Organizers (pools).
//-----------------------------------------------------------------------------

void block_chain::organize(block_const_ptr block, result_handler handler)
{
    orphan_manager_.organize(block, handler);
}

void block_chain::organize(transaction_const_ptr transaction,
    transaction_store_handler handler)
{
    // This is a simplification for the blockchain interface.
    const auto unhandled = [](code) {};

    // Notify only on completion of validation and accept/reject in pool.
    transaction_pool_.organize(transaction, unhandled, handler);
}

// Properties (thread safe).
// ----------------------------------------------------------------------------

const settings& block_chain::chain_settings() const
{
    return settings_;
}

// private
bool block_chain::stopped() const
{
    return stopped_;
}

// Sequential locking helpers.
// ----------------------------------------------------------------------------

// Use to create flush lock scope around multiple closely-spaced inserts.
bool block_chain::begin_writes()
{
    //vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    return database_.flush_lock();
}

bool block_chain::end_writes()
{
    return database_.flush_unlock();
    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
}

// private
template <typename Reader>
void block_chain::read_serial(const Reader& reader) const
{
    while (true)
    {
        // Get a read handle.
        const auto sequence = database_.begin_read();

        // If read handle indicates write and reader finishes false, wait.
        if (!database_.is_write_locked(sequence) && reader(sequence))
            break;

        // Sleep while waiting for write to complete.
        std::this_thread::sleep_for(spin_lock_sleep_);
    }
}

// private
template <typename Handler, typename... Args>
bool block_chain::finish_read(handle sequence, Handler handler,
    Args... args) const
{
    // If the read sequence was interrupted by a write, return false (wait). 
    if (!database_.is_read_valid(sequence))
        return false;

    // Handle the read (done).
    // Do not forward args, callers using smart pointer returns.
    handler(args...);
    return true;
}

// Utilities.
//-----------------------------------------------------------------------------

hash_list block_chain::to_hashes(const block_result& result)
{
    const auto count = result.transaction_count();
    hash_list hashes;
    hashes.reserve(count);

    for (size_t index = 0; index < count; ++index)
        hashes.push_back(result.transaction_hash(index));

    return hashes;
}

} // namespace blockchain
} // namespace libbitcoin
