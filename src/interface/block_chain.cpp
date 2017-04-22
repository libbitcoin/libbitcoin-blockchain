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
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::config;
using namespace bc::message;
using namespace bc::database;
using namespace std::placeholders;

#define NAME "block_chain"

block_chain::block_chain(threadpool& pool,
    const blockchain::settings& chain_settings,
    const database::settings& database_settings, bool relay_transactions)
  : stopped_(true),
    settings_(chain_settings),
    spin_lock_sleep_(asio::milliseconds(1)),
    chain_state_populator_(*this, chain_settings),
    database_(database_settings),
    validation_mutex_(database_settings.flush_writes && relay_transactions),
    priority_pool_(thread_ceiling(chain_settings.cores),
        priority(chain_settings.priority)),
    dispatch_(priority_pool_, NAME "_priority"),
    transaction_organizer_(validation_mutex_, dispatch_, pool, *this,
        chain_settings),
    block_organizer_(validation_mutex_, dispatch_, pool, *this, chain_settings,
        relay_transactions)
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

bool block_chain::get_branch_work(uint256_t& out_work,
    const uint256_t& maximum, size_t from_height) const
{
    size_t top;
    if (!database_.blocks().top(top))
        return false;

    out_work = 0;
    for (auto height = from_height; height <= top && out_work < maximum;
        ++height)
    {
        const auto result = database_.blocks().get(height);
        if (!result)
            return false;

        out_work += chain::block::proof(result.bits());
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
    size_t branch_height, bool require_confirmed) const
{
    // This includes a cached value for spender height (or not_spent).
    // Get the highest tx with matching hash, at or below the branch height.
    return database_.transactions().get_output(out_output, out_height,
        out_coinbase, outpoint, branch_height, require_confirmed);
}

bool block_chain::get_is_unspent_transaction(const hash_digest& hash,
    size_t branch_height, bool require_confirmed) const
{
    const auto result = database_.transactions().get(hash, branch_height,
        require_confirmed);

    return result && !result.is_spent(branch_height);
}

bool block_chain::get_transaction_position(size_t& out_height,
    size_t& out_position, const hash_digest& hash,
    bool require_confirmed) const
{
    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
        return false;

    out_height = result.height();
    out_position = result.position();
    return true;
}

transaction_ptr block_chain::get_transaction(size_t& out_block_height,
    const hash_digest& hash, bool require_confirmed) const
{
    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
        return nullptr;

    out_block_height = result.height();
    return std::make_shared<transaction>(result.transaction());
}

// Writers
// ----------------------------------------------------------------------------

bool block_chain::begin_insert() const
{
    return database_.begin_insert();
}

bool block_chain::end_insert() const
{
    return database_.end_insert();
}

bool block_chain::insert(block_const_ptr block, size_t height)
{
    return database_.insert(*block, height) == error::success;
}

void block_chain::push(transaction_const_ptr tx, dispatcher&,
    result_handler handler)
{
    // Transaction push is currently sequential so dispatch is not used.
    handler(database_.push(*tx, chain_state()->enabled_forks()));
}

void block_chain::reorganize(const checkpoint& fork_point,
    block_const_ptr_list_const_ptr incoming_blocks,
    block_const_ptr_list_ptr outgoing_blocks, dispatcher& dispatch,
    result_handler handler)
{
    if (incoming_blocks->empty())
    {
        handler(error::operation_failed);
        return;
    }

    // The top (back) block is used to update the chain state.
    const auto complete =
        std::bind(&block_chain::handle_reorganize,
            this, _1, incoming_blocks->back(), handler);

    database_.reorganize(fork_point, incoming_blocks, outgoing_blocks,
        dispatch, complete);
}

void block_chain::handle_reorganize(const code& ec, block_const_ptr top,
    result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    if (!top->validation.state)
    {
        handler(error::operation_failed);
        return;
    }

    set_chain_state(top->validation.state);
    last_block_.store(top);

    handler(error::success);
}

// Properties.
// ----------------------------------------------------------------------------

// For tx validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(pool_state_mutex_);

    // Initialized on start and updated after each successful organization.
    return pool_state_;
    ///////////////////////////////////////////////////////////////////////////
}

// For block validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state(
    branch::const_ptr branch) const
{
    // Promote from cache if branch is same height as pool (most typical).
    // Generate from branch/store if the promotion is not successful.
    // If the organize is successful pool state will be updated accordingly.
    return chain_state_populator_.populate(chain_state(), branch);
}

// private.
code block_chain::set_chain_state(chain::chain_state::ptr previous)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(pool_state_mutex_);

    pool_state_ = chain_state_populator_.populate(previous);
    return pool_state_ ? error::success : error::operation_failed;
    ///////////////////////////////////////////////////////////////////////////
}

// ============================================================================
// SAFE CHAIN
// ============================================================================

// Startup and shutdown.
// ----------------------------------------------------------------------------

bool block_chain::start()
{
    stopped_ = false;

    if (!database_.open())
        return false;

    // Initialize chain state after database start but before organizers.
    pool_state_ = chain_state_populator_.populate();

    return pool_state_ && transaction_organizer_.start() &&
        block_organizer_.start();
}

bool block_chain::stop()
{
    stopped_ = true;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    validation_mutex_.lock_high_priority();

    // This cannot call organize or stop (lock safe).
    auto result = transaction_organizer_.stop() && block_organizer_.stop();

    // The priority pool must not be stopped while organizing.
    priority_pool_.shutdown();

    validation_mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////
    return result;
}

// Close is idempotent and thread safe.
// Optional as the blockchain will close on destruct.
bool block_chain::close()
{
    const auto result = stop();
    priority_pool_.join();
    return result && database_.close();
}

block_chain::~block_chain()
{
    close();
}

// Queries.
// ----------------------------------------------------------------------------

void block_chain::fetch_block(size_t height, block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    // Try the last block first.
    const auto last = last_block_.load();
    BITCOIN_ASSERT(!last || last->validation.state);

    if (last && last->validation.state->height() == height)
    {
        // TODO: create chain::block::clean_copy() method.
        const auto copy = std::make_shared<message::block>(*last);
        copy->validation.state.reset();
        handler(error::success, copy, height);
        return;
    }

    const auto block_result = database_.blocks().get(height);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    BITCOIN_ASSERT(block_result.height() == height);
    const auto tx_hashes = block_result.transaction_hashes();
    const auto count = tx_hashes.size();
    const auto& tx_store = database_.transactions();
    transaction::list txs;
    txs.reserve(count);
    DEBUG_ONLY(size_t position = 0;)

    for (const auto& hash: tx_hashes)
    {
        const auto tx_result = tx_store.get(hash, max_size_t, true);

        if (!tx_result)
        {
            handler(error::operation_failed, nullptr, 0);
            return;
        }

        BITCOIN_ASSERT(tx_result.height() == height);
        BITCOIN_ASSERT(tx_result.position() == position++);
        txs.push_back(tx_result.transaction());
    }

    const auto block = std::make_shared<message::block>(block_result.header(),
        std::move(txs));
    handler(error::success, block, height);
}

void block_chain::fetch_block(const hash_digest& hash,
    block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    // Try the last block first.
    const auto last = last_block_.load();
    BITCOIN_ASSERT(!last || last->validation.state);

    if (last && last->hash() == hash)
    {
        // TODO: create chain::block::clean_copy() method.
        const auto height = last->validation.state->height();
        const auto copy = std::make_shared<message::block>(*last);
        copy->validation.state.reset();
        handler(error::success, copy, height);
        return;
    }

    const auto block_result = database_.blocks().get(hash);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto block_height = block_result.height();
    const auto tx_hashes = block_result.transaction_hashes();
    const auto count = tx_hashes.size();
    const auto& tx_store = database_.transactions();
    transaction::list txs;
    txs.reserve(count);
    DEBUG_ONLY(size_t position = 0;)

    for (const auto& hash: tx_hashes)
    {
        const auto tx_result = tx_store.get(hash, max_size_t, true);

        if (!tx_result)
        {
            handler(error::operation_failed, nullptr, 0);
            return;
        }

        BITCOIN_ASSERT(tx_result.height() == block_height);
        BITCOIN_ASSERT(tx_result.position() == position++);
        txs.push_back(tx_result.transaction());
    }

    const auto block = std::make_shared<message::block>(block_result.header(),
        std::move(txs));
    handler(error::success, block, block_height);
}

void block_chain::fetch_block_header(size_t height,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(height);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto header = std::make_shared<message::header>(result.header());
    handler(error::success, header, result.height());
}

void block_chain::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto header = std::make_shared<message::header>(result.header());
    handler(error::success, header, result.height());
}

void block_chain::fetch_merkle_block(size_t height,
    merkle_block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(height);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto merkle = std::make_shared<merkle_block>(result.header(),
        result.transaction_count(), result.transaction_hashes(), data_chunk{});
    handler(error::success, merkle, result.height());
}

void block_chain::fetch_merkle_block(const hash_digest& hash,
    merkle_block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto merkle = std::make_shared<merkle_block>(result.header(),
        result.transaction_count(), result.transaction_hashes(), data_chunk{});
    handler(error::success, merkle, result.height());
}

void block_chain::fetch_compact_block(size_t height,
    compact_block_fetch_handler handler) const
{
    // TODO: implement compact blocks.
    handler(error::not_implemented, {}, 0);
}

void block_chain::fetch_compact_block(const hash_digest& hash,
    compact_block_fetch_handler handler) const
{
    // TODO: implement compact blocks.
    handler(error::not_implemented, {}, 0);
}

void block_chain::fetch_block_height(const hash_digest& hash,
    block_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, 0);
        return;
    }

    handler(error::success, result.height());
}

void block_chain::fetch_last_height(last_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    size_t last_height;

    if (!database_.blocks().top(last_height))
    {
        handler(error::not_found, 0);
        return;
    }

    handler(error::success, last_height);
}

void block_chain::fetch_transaction(const hash_digest& hash,
    bool require_confirmed, transaction_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0, 0);
        return;
    }

    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
    {
        handler(error::not_found, nullptr, 0, 0);
        return;
    }

    const auto tx = std::make_shared<transaction>(result.transaction());
    handler(error::success, tx, result.position(), result.height());
}

// This is only used for the server API, need to document sentinel/forks.
// This is same as fetch_transaction but skips the tx payload.
void block_chain::fetch_transaction_position(const hash_digest& hash,
    bool require_confirmed, transaction_index_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, 0, 0);
        return;
    }

    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
    {
        handler(error::not_found, 0, 0);
        return;
    }

    handler(error::success, result.position(), result.height());
}

// This is not called by any libbitcoin library.
// Spender height is unguarded and will be inconsistent if read during write.
void block_chain::fetch_output(const chain::output_point& outpoint,
    bool require_confirmed, output_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto result = database_.transactions().get(outpoint.hash(),
        max_size_t, require_confirmed);

    if (!result)
    {
        handler(error::not_found, {});
        return;
    }

    auto output = result.output(outpoint.index());
    const auto ec = output.is_valid() ? error::success : error::not_found;
    handler(ec, std::move(output));
}

void block_chain::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    auto point = database_.spends().get(outpoint);

    if (point.hash() == null_hash)
    {
        handler(error::not_found, {});
        return;
    }

    handler(error::success, std::move(point));
}

void block_chain::fetch_history(const short_hash& address_hash, size_t limit,
    size_t from_height, history_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::success, database_.history().get(address_hash, limit,
        from_height));
}

void block_chain::fetch_stealth(const binary& filter, size_t from_height,
    stealth_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::success, database_.stealth().scan(filter, from_height));
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

    // Find the start block height.
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

    // The first block requested is always one after the start block.
    auto first = safe_add(start, size_t(1));

    // The maximum last block requested is 500 after first.
    auto last = safe_add(first, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not on chain we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash());

        // Otherwise limit the last height to the stop block height.
        // If last precedes first floor_subtract will handle below.
        if (result)
            last = std::min(result.height(), last);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not on chain we ignore it.
        const auto result = database_.blocks().get(threshold);

        // Otherwise limit the first height to the threshold block height.
        // If first exceeds last floor_subtract will handle below.
        if (result)
            first = std::max(result.height(), first);
    }

    auto hashes = std::make_shared<inventory>();
    hashes->inventories().reserve(floor_subtract(last, first));

    // Build the hash list until we hit last or the blockchain top.
    for (auto index = first; index < last; ++index)
    {
        const auto result = database_.blocks().get(index);

        // If not found then we are at our top.
        if (!result)
        {
            hashes->inventories().shrink_to_fit();
            break;
        }

        static const auto id = inventory::type_id::block;
        hashes->inventories().emplace_back(id, result.header().hash());
    }

    handler(error::success, std::move(hashes));
}

// This may execute over 2000 queries.
void block_chain::fetch_locator_block_headers(get_headers_const_ptr locator,
    const hash_digest& threshold, size_t limit,
    locator_block_headers_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.

    // Find the start block height.
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

    // The first block requested is always one after the start block.
    auto first = safe_add(start, size_t(1));

    // The maximum last block requested is 2000 after first.
    auto last = safe_add(first, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not on chain we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash());

        // Otherwise limit the last height to the stop block height.
        // If last precedes first floor_subtract will handle below.
        if (result)
            last = std::min(result.height(), last);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not on chain we ignore it.
        const auto result = database_.blocks().get(threshold);

        // Otherwise limit the first height to the threshold block height.
        // If first exceeds last floor_subtract will handle below.
        if (result)
            first = std::max(result.height(), first);
    }

    auto headers = std::make_shared<message::headers>();
    headers->elements().reserve(floor_subtract(last, first));

    // Build the hash list until we hit last or the blockchain top.
    for (auto index = first; index < last; ++index)
    {
        const auto result = database_.blocks().get(index);

        // If not found then we are at our top.
        if (!result)
        {
            headers->elements().shrink_to_fit();
            break;
        }

        headers->elements().push_back(result.header());
    }

    handler(error::success, std::move(headers));
}

// Transaction Pool.
//-----------------------------------------------------------------------------

// Same as fetch_mempool but also optimized for maximum possible block fee as
// limited by total bytes and signature operations.
void block_chain::fetch_template(merkle_block_fetch_handler handler) const
{
    transaction_organizer_.fetch_template(handler);
}

// Fetch a set of currently-valid unconfirmed txs in dependency order.
// All txs satisfy the fee minimum and are valid at the next chain state.
// The set of blocks is limited in count to size. The set may have internal
// dependencies but all inputs must be satisfied at the current height.
void block_chain::fetch_mempool(size_t count_limit, uint64_t minimum_fee,
    inventory_fetch_handler handler) const
{
    transaction_organizer_.fetch_mempool(count_limit, handler);
}

// Filters.
//-----------------------------------------------------------------------------

// This may execute up to 500 queries.
// This filters against the block pool and then the block chain.
void block_chain::filter_blocks(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Filter through block pool first.
    block_organizer_.filter(message);
    auto& inventories = message->inventories();
    const auto& blocks = database_.blocks();

    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_block_type() && blocks.get(it->hash()))
            it = inventories.erase(it);
        else
            ++it;
    }

    handler(error::success);
}

// This filters against all transactions (confirmed and unconfirmed).
void block_chain::filter_transactions(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    auto& inventories = message->inventories();
    const auto& transactions = database_.transactions();

    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_transaction_type() &&
            get_is_unspent_transaction(it->hash(), max_size_t, false))
            it = inventories.erase(it);
        else
            ++it;
    }

    handler(error::success);
}

// Subscribers.
//-----------------------------------------------------------------------------

void block_chain::subscribe_blockchain(reorganize_handler&& handler)
{
    // Pass this through to the organizer, which issues the notifications.
    block_organizer_.subscribe(std::move(handler));
}

void block_chain::subscribe_transaction(transaction_handler&& handler)
{
    // Pass this through to the tx organizer, which issues the notifications.
    transaction_organizer_.subscribe(std::move(handler));
}

void block_chain::unsubscribe()
{
    block_organizer_.unsubscribe();
    transaction_organizer_.unsubscribe();
}

// Organizers.
//-----------------------------------------------------------------------------

void block_chain::organize(block_const_ptr block, result_handler handler)
{
    // This cannot call organize or stop (lock safe).
    block_organizer_.organize(block, handler);
}

void block_chain::organize(transaction_const_ptr tx, result_handler handler)
{
    // This cannot call organize or stop (lock safe).
    transaction_organizer_.organize(tx, handler);
}

// Properties (thread safe).
// ----------------------------------------------------------------------------

const settings& block_chain::chain_settings() const
{
    return settings_;
}

// protected
bool block_chain::stopped() const
{
    return stopped_;
}

// Locking helpers.
// ----------------------------------------------------------------------------
// private

template <typename Reader>
void block_chain::read_serial(const Reader& reader) const
{
    while (true)
    {
        // Get a read handle.
        const auto sequence = database_.begin_read();

        // If read handle indicates write or reader finishes false, wait.
        if (!database_.is_write_locked(sequence) && reader(sequence))
            break;

        // Sleep while waiting for write to complete.
        std::this_thread::sleep_for(spin_lock_sleep_);
    }
}

template <typename Handler, typename... Args>
bool block_chain::finish_read(handle sequence, Handler handler,
    Args... args) const
{
    // If the read sequence was interrupted by a write, return false (wait).
    if (!database_.is_read_valid(sequence))
        return false;

    // Handle the read (done).
    // To forward args we would need to use std::bind here, but not necessary
    // because all parameterizations use smart pointers or integral types.
    handler(args...);
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
