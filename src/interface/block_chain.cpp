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

static const auto hour_seconds = 3600u;

block_chain::block_chain(threadpool& pool,
    const blockchain::settings& chain_settings,
    const database::settings& database_settings)
  : stopped_(true),
    settings_(chain_settings),
    notify_limit_seconds_(chain_settings.notify_limit_hours * hour_seconds),
    chain_state_populator_(*this, chain_settings),
    database_(database_settings),

    // TODO: tune/configure this.
    validation_mutex_(database_settings.flush_writes),

    priority_pool_(thread_ceiling(chain_settings.cores),
        priority(chain_settings.priority)),
    dispatch_(priority_pool_, NAME "_priority"),
    header_organizer_(validation_mutex_, dispatch_, pool, *this,
        chain_settings),
    block_organizer_(validation_mutex_, dispatch_, pool, *this,
        chain_settings),
    transaction_organizer_(validation_mutex_, dispatch_, pool, *this,
        chain_settings)
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
    return database_.blocks().get(block_hash, true);
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
    auto result = database_.blocks().get(block_hash, true);
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
    uint32_t& out_median_time_past, bool& out_coinbase,
    const chain::output_point& outpoint, size_t branch_height,
    bool require_confirmed) const
{
    // This includes a cached value for spender height (or not_spent).
    // Get the highest tx with matching hash, at or below the branch height.
    return database_.transactions().get_output(out_output, out_height,
        out_median_time_past, out_coinbase, outpoint, branch_height,
        require_confirmed);
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

////transaction_ptr block_chain::get_transaction(size_t& out_block_height,
////    const hash_digest& hash, bool require_confirmed) const
////{
////    const auto result = database_.transactions().get(hash, max_size_t,
////        require_confirmed);
////
////    if (!result)
////        return nullptr;
////
////    out_block_height = result.height();
////    return std::make_shared<transaction>(result.transaction());
////}

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
    const auto state = tx->validation.state;

    if (!state)
    {
        handler(error::operation_failed);
        return;
    }

    last_transaction_.store(tx);

    // Transaction push is currently sequential so dispatch is not used.
    handler(database_.push(*tx, state->enabled_forks()));
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

    set_pool_state(*top->validation.state);
    last_block_.store(top);

    handler(error::success);
}

// Properties.
// ----------------------------------------------------------------------------
// TODO: move pool_state_ into the new transaction_pool.

// For tx validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state() const
{
    // If this returns empty pointer it indicates start failed.
    // The pool state is computed and cached upon top block update.
    return pool_state_.load();
}

// For header validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state(header_const_ptr header) const
{
    // If this returns empty pointer it indicates store corruption.
    // Promote from header.parent in the header-pool or generate from store if
    // the header is a new branch, otherwise fail (orphan).
    //=========================================================================
    //=========================================================================
    // TODO: replace *chain_state() with pool parent state if exists...
    // (otherwise must update parameterization).
    //=========================================================================
    //=========================================================================
    return chain_state_populator_.populate(*chain_state(), header);
}

// For block validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state(
    branch::const_ptr branch) const
{
    // If this returns empty pointer it indicates store corruption.
    // Promote from tx pool if branch is same height as pool (most typical).
    // Generate from branch/store if the promotion is not successful.
    // If the organize is successful pool state will be updated accordingly.
    return chain_state_populator_.populate(*chain_state(), branch);
}

// private.
void block_chain::set_pool_state(const chain::chain_state& top)
{
    // Promotion always succeeds.
    pool_state_.store(std::make_shared<chain::chain_state>(top));
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

    // Initialize chain state after database start and before organizers.
    pool_state_.store(chain_state_populator_.populate());

    return pool_state_.load() &&
        transaction_organizer_.start() &&
        header_organizer_.start() &&
        block_organizer_.start();
}

bool block_chain::stop()
{
    stopped_ = true;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    validation_mutex_.lock_high_priority();

    // This cannot call organize or stop (lock safe).
    auto result = 
        transaction_organizer_.stop() &&
        header_organizer_.stop() &&
        block_organizer_.stop();

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

// private
bool block_chain::get_transactions(transaction::list& out_transactions,
    const offset_list& tx_offsets) const
{
    out_transactions.reserve(tx_offsets.size());
    const auto& tx_store = database_.transactions();

    for (const auto offset: tx_offsets)
    {
        // We don't care about tx metadata since it's block-aligned.
        const auto result = tx_store.get(offset);

        if (!result)
            return false;

        out_transactions.push_back(result.transaction());
    }

    return true;
}

// private
bool block_chain::get_transaction_hashes(hash_list& out_hashes,
    const offset_list& tx_offsets) const
{
    out_hashes.reserve(tx_offsets.size());
    const auto& tx_store = database_.transactions();

    for (const auto offset: tx_offsets)
    {
        // We don't care about tx metadata since it's block-aligned.
        const auto result = tx_store.get(offset);

        if (!result)
            return false;

        out_hashes.push_back(result.hash());
    }

    return true;
}

void block_chain::fetch_block(size_t height, block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto cached = last_block_.load();

    // Try the cached block first.
    if (cached && cached->validation.state &&
        cached->validation.state->height() == height)
    {
        handler(error::success, cached, height);
        return;
    }

    const auto block_result = database_.blocks().get(height);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    transaction::list txs;
    BITCOIN_ASSERT(block_result.height() == height);

    if (!get_transactions(txs, block_result.transaction_offsets()))
    {
        handler(error::operation_failed, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<const block>(block_result.header(),
        std::move(txs));
    handler(error::success, message, height);
}

void block_chain::fetch_block(const hash_digest& hash,
    block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto cached = last_block_.load();

    // Try the cached block first.
    if (cached && cached->validation.state && cached->hash() == hash)
    {
        handler(error::success, cached, cached->validation.state->height());
        return;
    }

    // TODO: parameterize require_confirmed.
    const auto block_result = database_.blocks().get(hash, true);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    transaction::list txs;

    if (!get_transactions(txs, block_result.transaction_offsets()))
    {
        handler(error::operation_failed, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<const block>(block_result.header(),
        std::move(txs));
    handler(error::success, message, block_result.height());
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

    BITCOIN_ASSERT(result.height() == height);
    const auto message = std::make_shared<header>(result.header());
    handler(error::success, message, height);
}

void block_chain::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    // TODO: parameterize require_confirmed.
    const auto result = database_.blocks().get(hash, true);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<header>(result.header());
    handler(error::success, message, result.height());
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

    hash_list hashes;
    BITCOIN_ASSERT(result.height() == height);

    if (!get_transaction_hashes(hashes, result.transaction_offsets()))
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto merkle = std::make_shared<merkle_block>(result.header(),
        hashes.size(), hashes, data_chunk{});
    handler(error::success, merkle, height);
}

void block_chain::fetch_merkle_block(const hash_digest& hash,
    merkle_block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    // TODO: parameterize require_confirmed.
    const auto result = database_.blocks().get(hash, true);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    hash_list hashes;

    if (!get_transaction_hashes(hashes, result.transaction_offsets()))
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto merkle = std::make_shared<merkle_block>(result.header(),
        hashes.size(), hashes, data_chunk{});
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

    // TODO: parameterize require_confirmed.
    const auto result = database_.blocks().get(hash, true);

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

    // Try the cached block first if confirmation is not required.
    if (!require_confirmed)
    {
        const auto cached = last_transaction_.load();

        if (cached && cached->validation.state && cached->hash() == hash)
        {
            ////LOG_INFO(LOG_BLOCKCHAIN) << "TX CACHE HIT";

            // Simulate the position and height overloading of the database.
            handler(error::success, cached, transaction_database::unconfirmed,
                cached->validation.state->height());
            return;
        }
    }

    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
    {
        handler(error::not_found, nullptr, 0, 0);
        return;
    }

    const auto tx = std::make_shared<const transaction>(result.transaction());
    handler(error::success, tx, result.position(), result.height());
}

// This is same as fetch_transaction but skips deserializing the tx payload.
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
        const auto result = database_.blocks().get(hash, true);
        if (result)
        {
            start = result.height();
            break;
        }
    }

    // The begin block requested is always one after the start block.
    auto begin = safe_add(start, size_t(1));

    // The maximum number of headers returned is 500.
    auto end = safe_add(begin, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not on chain we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash(), true);

        // Otherwise limit the end height to the stop block height.
        // If end precedes begin floor_subtract will handle below.
        if (result)
            end = std::min(result.height(), end);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not on chain we ignore it.
        const auto result = database_.blocks().get(threshold, true);

        // Otherwise limit the begin height to the threshold block height.
        // If begin exceeds end floor_subtract will handle below.
        if (result)
            begin = std::max(result.height(), begin);
    }

    auto hashes = std::make_shared<inventory>();
    hashes->inventories().reserve(floor_subtract(end, begin));

    // Build the hash list until we hit end or the blockchain top.
    for (auto height = begin; height < end; ++height)
    {
        const auto result = database_.blocks().get(height);

        // If not found then we are at our top.
        if (!result)
        {
            hashes->inventories().shrink_to_fit();
            break;
        }

        static const auto id = inventory::type_id::block;
        hashes->inventories().emplace_back(id, result.hash());
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
        const auto result = database_.blocks().get(hash, true);
        if (result)
        {
            start = result.height();
            break;
        }
    }

    // The begin block requested is always one after the start block.
    auto begin = safe_add(start, size_t(1));

    // The maximum number of headers returned is 2000.
    auto end = safe_add(begin, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not on chain we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash(), true);

        // Otherwise limit the end height to the stop block height.
        // If end precedes begin floor_subtract will handle below.
        if (result)
            end = std::min(result.height(), end);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not on chain we ignore it.
        const auto result = database_.blocks().get(threshold, true);

        // Otherwise limit the begin height to the threshold block height.
        // If begin exceeds end floor_subtract will handle below.
        if (result)
            begin = std::max(result.height(), begin);
    }

    auto message = std::make_shared<headers>();
    message->elements().reserve(floor_subtract(end, begin));

    // Build the hash list until we hit end or the blockchain top.
    for (auto height = begin; height < end; ++height)
    {
        const auto result = database_.blocks().get(height);

        // If not found then we are at our top.
        if (!result)
        {
            message->elements().shrink_to_fit();
            break;
        }

        message->elements().push_back(result.header());
    }

    handler(error::success, std::move(message));
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

    auto message = std::make_shared<get_blocks>();
    auto& hashes = message->start_hashes();
    hashes.reserve(heights.size());

    for (const auto height: heights)
    {
        const auto result = database_.blocks().get(height);

        if (!result)
        {
            handler(error::not_found, nullptr);
            break;
        }

        hashes.push_back(result.hash());
    }

    handler(error::success, message);
}

// This may generally execute 29+ queries.
void block_chain::fetch_header_locator(const block::indexes& heights,
    header_locator_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    auto message = std::make_shared<get_headers>();
    auto& hashes = message->start_hashes();
    hashes.reserve(heights.size());

    for (const auto height: heights)
    {
        const auto result = database_.blocks().get(height);

        if (!result)
        {
            handler(error::not_found, nullptr);
            break;
        }

        hashes.push_back(result.hash());
    }

    handler(error::success, message);
}

// Server Queries.
//-----------------------------------------------------------------------------

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

    handler(error::success, database_.stealth().get(filter, from_height));
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

// This may execute up to 500 queries (protocol limit).
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
        if (it->is_block_type() && blocks.get(it->hash(), true))
            it = inventories.erase(it);
        else
            ++it;
    }

    handler(error::success);
}

// This may execute up to 50,000 queries (protocol limit).
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
    // This cannot call organize oand must progress (lock safe).
    block_organizer_.organize(block, handler);
}

void block_chain::organize(header_const_ptr header, result_handler handler)
{
    // This cannot call organize oand must progress (lock safe).
    header_organizer_.organize(header, handler);
}

void block_chain::organize(transaction_const_ptr tx, result_handler handler)
{
    // This cannot call organize oand must progress (lock safe).
    transaction_organizer_.organize(tx, handler);
}

// Properties (thread safe).
// ----------------------------------------------------------------------------

// This satisfies the same virtual method on both safe_chain and fast_chain.
bool block_chain::is_stale() const
{
    // If there is no limit set the chain is never considered stale.
    if (notify_limit_seconds_ == 0)
        return false;

    // The chain is stale after start until first new block is cached.
    const auto top = last_block_.load();
    const auto timestamp = top ? top->header().timestamp() : uint32_t(0);
    return timestamp < floor_subtract(zulu_time(), notify_limit_seconds_);
}

const settings& block_chain::chain_settings() const
{
    return settings_;
}

// protected
bool block_chain::stopped() const
{
    return stopped_;
}

} // namespace blockchain
} // namespace libbitcoin
