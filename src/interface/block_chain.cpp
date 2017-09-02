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
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::message;
using namespace bc::database;
using namespace std::placeholders;

#define NAME "block_chain"

block_chain::block_chain(threadpool& pool,
    const blockchain::settings& chain_settings,
    const database::settings& database_settings)
  : stopped_(true),
    settings_(chain_settings),
    chain_state_populator_(*this, chain_settings),
    database_(database_settings),

    // TODO: tune/configure this.
    validation_mutex_(database_settings.flush_writes),

    priority_pool_(thread_ceiling(chain_settings.cores),
        priority(chain_settings.priority)),
    dispatch_(priority_pool_, NAME "_priority"),
    header_organizer_(validation_mutex_, dispatch_, pool, *this,
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

size_t block_chain::get_fork_point() const
{
    return database_.blocks().fork_point();
}

bool block_chain::get_top(config::checkpoint& out_checkpoint,
    bool block_index) const
{
    size_t height;
    hash_digest hash;

    if (!get_top_height(height, block_index) ||
        !get_block_hash(hash, height, block_index))
        return false;

    out_checkpoint = { hash, height };
    return true;
}

bool block_chain::get_top_height(size_t& out_height, bool block_index) const
{
    return database_.blocks().top(out_height, block_index);
}

bool block_chain::get_pending_block_hash(hash_digest& out_hash, bool& out_empty,
    size_t height) const
{
    const auto result = database_.blocks().get(height, false);

    if (!result || !is_pent(result.state()))
        return false;

    out_hash = result.hash();
    out_empty = result.transaction_count() == 0;
    return true;
}

// Header indexing controlled by fork_height.
// All blocks have height but this limits to indexed|confirmed as applicable.
bool block_chain::get_block_height(size_t& out_height,
    const hash_digest& block_hash, size_t fork_height) const
{
    auto result = database_.blocks().get(block_hash);

    if (!result)
        return false;

    const auto state = result.state();
    const auto height = result.height();
    const auto relevant = height <= fork_height;
    const auto confirmed = is_indexed(state) ||
        (is_confirmed(state) && relevant);

    // Cannot have index height unless confirmed in an index.
    if (!confirmed)
        return false;

    out_height = height;
    return true;
}

bool block_chain::get_block_hash(hash_digest& out_hash, size_t height,
    bool block_index) const
{
    const auto result = database_.blocks().get(height, block_index);

    if (!result)
        return false;

    out_hash = result.hash();
    return true;
}

bool block_chain::get_block_error(code& out_error,
    const hash_digest& block_hash) const
{
    auto result = database_.blocks().get(block_hash);

    if (!result)
        return false;

    out_error = result.error();
    return true;
}

bool block_chain::get_transaction_error(code& out_error,
    const hash_digest& tx_hash) const
{
    auto result = database_.transactions().get(tx_hash);

    if (!result)
        return false;

    out_error = result.error();
    return true;
}

bool block_chain::get_bits(uint32_t& out_bits, size_t height,
    bool block_index) const
{
    auto result = database_.blocks().get(height, block_index);

    if (!result)
        return false;

    out_bits = result.bits();
    return true;
}

bool block_chain::get_timestamp(uint32_t& out_timestamp, size_t height,
    bool block_index) const
{
    auto result = database_.blocks().get(height, block_index);

    if (!result)
        return false;

    out_timestamp = result.timestamp();
    return true;
}

bool block_chain::get_version(uint32_t& out_version, size_t height,
    bool block_index) const
{
    auto result = database_.blocks().get(height, block_index);

    if (!result)
        return false;

    out_version = result.version();
    return true;
}

bool block_chain::get_work(uint256_t& out_work, const uint256_t& maximum,
    size_t above_height, bool block_index) const
{
    size_t top;
    out_work = 0;

    if (!database_.blocks().top(top, block_index))
        return false;

    for (auto height = top; height > above_height && out_work < maximum;
        --height)
    {
        const auto result = database_.blocks().get(height, block_index);

        if (!result)
            return false;

        out_work += chain::header::proof(result.bits());
    }

    return true;
}

// Header indexing controlled by fork_height.
// TODO: move into block database (see populate_output/get_output below).
void block_chain::populate_header(const chain::header& header,
    size_t fork_height) const
{
    const auto result = database_.blocks().get(header.hash());

    // Default values are correct for indication of not found.
    if (!result)
        return;

    const auto state = result.state();
    const auto height = result.height();
    const auto relevant = height <= fork_height;

    // All headers have a fixed height, independent of indexing.
    header.validation.height = height;

    // Transactions are populated (count population is atomic).
    header.validation.populated = result.transaction_count() != 0;

    // Stored headers are always valid, error refers to the block.
    header.validation.pooled = true;

    // Duplicate implies that a new header should be ignored.
    header.validation.duplicate = is_indexed(state) ||
        (is_confirmed(state) && relevant);

    // An error results from block validation failure after tx download.
    header.validation.error = result.error();
}

// Header indexing or pool controlled by fork_height.
// TODO: move into tx database (see populate_output/get_output below).
void block_chain::populate_transaction(const chain::transaction& tx,
    uint32_t forks, size_t fork_height) const
{
    const auto result = database_.transactions().get(tx.hash());

    // Default values are correct for indication of not found.
    if (!result)
        return;

    const auto state = result.state();
    const auto height = result.height();
    const auto relevant = height <= fork_height;
    const auto for_pool = fork_height == max_size_t;

    // Indexed transactions retain forks in height.
    const auto fork_valid = forks == height;

    // Confirmation is relative to header index (not block index).
    const auto indexed =
        (state == transaction_state::indexed) ||
        (state == transaction_state::confirmed && relevant);

    // Pooled implies that the tx should not be revalidated.
    // Fork-valid is not applicable to indexed txs, as forks are not stored.
    tx.validation.pooled = !indexed && fork_valid;

    // Duplicate implies that a new tx should be ignored.
    tx.validation.duplicate = indexed || (for_pool && fork_valid);

    // This is currently always success, since there is no tx invalid state.
    tx.validation.error = result.error();

    // This allows the existing transaction to be associated to a block.
    tx.validation.offset = result.offset();

    //*************************************************************************
    // CONSENSUS: The satoshi hard fork that reverts BIP30 after BIP34 makes a
    // spentness test moot as tx hash collision is presumed impossible. Yet we
    // prefer correct behavior in the "impossible" case vs. deleting money.
    // But we can reject the rare scenario for the tx pool as an optimization.
    //*************************************************************************
    if (tx.validation.duplicate && !for_pool && result.is_spent(fork_height))
    {
        // Treat a spent duplicate as if it did not exist.
        // The original tx will not be queryable independent of the block.
        // The original tx's block linkage is unbroken by accepting duplicate.
        // If the new block is popped the original tx resurfaces automatically.
        tx.validation.pooled = false;
        tx.validation.duplicate = false;
        tx.validation.error = error::success;
        tx.validation.offset = transaction::validation::undetermined_offset;
    }
}

void block_chain::populate_output(const chain::output_point& outpoint,
    size_t fork_height) const
{
    database_.transactions().get_output(outpoint, fork_height);
}

uint8_t block_chain::get_block_state(const hash_digest& block_hash) const
{
    return database_.blocks().get(block_hash).state();
}

database::transaction_state block_chain::get_transaction_state(
    const hash_digest& tx_hash) const
{
    return database_.transactions().get(tx_hash).state();
}

// Writers
// ----------------------------------------------------------------------------

void block_chain::reindex(const config::checkpoint& fork_point,
    header_const_ptr_list_const_ptr incoming,
    header_const_ptr_list_ptr outgoing, dispatcher& dispatch,
    result_handler handler)
{
    if (incoming->empty())
    {
        handler(error::operation_failed);
        return;
    }

    // The top (back) block is used to update the chain state.
    const auto complete =
        std::bind(&block_chain::handle_reindex,
            this, _1, incoming->back(), handler);

    database_.reorganize(fork_point, incoming, outgoing, dispatch, complete);
}

// Due to accept/population bypass, chain_state may not be populated.
void block_chain::handle_reindex(const code& ec,
    header_const_ptr top_header, result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    // We could alternatively just read the pool state from last header cache.
    set_header_pool_state(top_header->validation.state);
    last_header_.store(top_header);
    handler(error::success);
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

bool block_chain::push(block_const_ptr block, size_t height)
{
    return database_.push(*block, height) == error::success;
}

// Properties.
// ----------------------------------------------------------------------------

// TODO: move header_pool_state_ into the header_pool.
chain::chain_state::ptr block_chain::header_pool_state() const
{
    // The header_pool state is computed and cached upon top header update.
    // Empty result indicates blockchain start failed (should not be here).
    return header_pool_state_.load();
}

// TODO: move transaction_pool_state_ into the new transaction_pool.
// For tx validator, call only from validate critical section.
chain::chain_state::ptr block_chain::transaction_pool_state() const
{
    // The pool state is computed and cached upon top block update.
    // Empty result indicates blockchain start failed (should not be here).
    return transaction_pool_state_.load();
}

// For block validator, call only from validate critical section.
chain::chain_state::ptr block_chain::chain_state(block_const_ptr block) const
{
    const auto height = block->header().validation.height;

    // Parent height is required.
    if (height == 0)
        return{};

    // TODO: bury this into chain state populator.
    const auto parent_height = height - 1u;
    const auto branch = std::make_shared<header_branch>(parent_height);
    branch->push(std::make_shared<const header>(block->header()));
    return chain_state(branch);
}

// For header validator, call only from validate critical section.
chain::chain_state::ptr block_chain::chain_state(
    header_branch::const_ptr branch) const
{
    // Get chain state for the last block in the branch.
    // Empty result indicates empty branch or missing branch header/ancestor.
    return chain_state_populator_.populate(branch);
}

// private.
bool block_chain::set_pool_states()
{
    set_header_pool_state(chain_state_populator_.populate(false));
    set_transaction_pool_state(chain_state_populator_.populate(true));

    // Empty state indicates empty chain or corruption (should not be here).
    return header_pool_state_.load() != nullptr &&
        transaction_pool_state_.load() != nullptr;
}

// private.
void block_chain::set_header_pool_state(chain::chain_state::ptr top)
{
    // Header pool state is that of the top indexed header.
    header_pool_state_.store(top);
}

// private.
void block_chain::set_transaction_pool_state(chain::chain_state::ptr top)
{
    // Promotion always succeeds.
    // Tx pool state is promoted from the state of the top confirmed block.
    transaction_pool_state_.store(std::make_shared<chain::chain_state>(*top));
}

bool block_chain::is_blocks_stale() const
{
    // The pool state is as fresh as the last (top) indexed block.
    const auto state = transaction_pool_state_.load();
    return state && state->is_stale();
}

bool block_chain::is_headers_stale() const
{
    // The header state is as fresh as the last (top) indexed header.
    const auto state = header_pool_state_.load();
    return state && state->is_stale();
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

    return set_pool_states() &&
        header_organizer_.start() &&
        ////block_organizer_.start() &&
        transaction_organizer_.start();
}

bool block_chain::stop()
{
    stopped_ = true;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    validation_mutex_.lock_high_priority();

    // This cannot call organize or stop (lock safe).
    auto result = 
        header_organizer_.stop() &&
        ////block_organizer_.stop() &&
        transaction_organizer_.stop();

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
    const offset_list& tx_offsets, bool witness) const
{
    out_transactions.reserve(tx_offsets.size());
    const auto& tx_store = database_.transactions();

    for (const auto offset: tx_offsets)
    {
        // We don't care about tx metadata since it's block-aligned.
        const auto result = tx_store.get(offset);

        if (!result)
            return false;

        out_transactions.push_back(result.transaction(witness));
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

void block_chain::fetch_block(size_t height, bool witness,
    block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    // TODO: not currently populated.
    const auto cached = last_block_.load();

    // Try the cached block first.
    if (cached && cached->header().validation.state &&
        cached->header().validation.state->height() == height)
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

    if (!get_transactions(txs, block_result.transaction_offsets(), witness))
    {
        handler(error::operation_failed, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<const block>(block_result.header(),
        std::move(txs));
    handler(error::success, message, height);
}

void block_chain::fetch_block(const hash_digest& hash, bool witness,
    block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    // TODO: not currently populated.
    const auto cached = last_block_.load();

    // Try the cached block first.
    if (cached && cached->header().validation.state && cached->hash() == hash)
    {
        const auto height = cached->header().validation.state->height();
        handler(error::success, cached, height);
        return;
    }

    const auto block_result = database_.blocks().get(hash);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    transaction::list txs;

    if (!get_transactions(txs, block_result.transaction_offsets(), witness))
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

    const auto result = database_.blocks().get(hash);

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

    const auto result = database_.blocks().get(hash);

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
    bool require_confirmed, bool witness,
    transaction_fetch_handler handler) const
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
            const auto height = cached->validation.state->height();
            handler(error::success, cached, 0, height);
            return;
        }
    }

    const auto result = database_.transactions().get(hash);

    if (!result || (require_confirmed && result.state() !=
        transaction_state::confirmed))
    {
        handler(error::not_found, nullptr, 0, 0);
        return;
    }

    // TODO: tx state may not be publishable.
    const auto tx = std::make_shared<const transaction>(
        result.transaction(witness));
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

    // Try the cached block first if confirmation is not required.
    if (!require_confirmed)
    {
        const auto cached = last_transaction_.load();

        if (cached && cached->validation.state && cached->hash() == hash)
        {
            ////LOG_INFO(LOG_BLOCKCHAIN) << "TX CACHE HIT";

            // Simulate the position and height overloading of the database.
            const auto height = cached->validation.state->height();
            handler(error::success, 0, height);
            return;
        }
    }

    const auto result = database_.transactions().get(hash);

    if (!result || (require_confirmed && result.state() !=
        transaction_state::confirmed))
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

    // BUGBUG: an intervening reorg can produce an invalid chain of hashes.
    // TODO: instead walk backwards using parent hash lookups. 

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.

    // Find the start block height.
    // If no start block is on our chain we start with block 0.
    // TODO: we could return error or empty and drop peer for missing genesis.
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

    // The begin block requested is always one after the start block.
    auto begin = safe_add(start, size_t(1));

    // The maximum number of headers returned is 500.
    auto end = safe_add(begin, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not confirmed we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash());

        // Otherwise limit the end height to the stop block height.
        // If end precedes begin floor_subtract will handle below.
        if (result)
            end = std::min(result.height(), end);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not confirmed we ignore it.
        const auto result = database_.blocks().get(threshold);

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

    // BUGBUG: an intervening reorg can produce an invalid chain of headers.
    // TODO: instead walk backwards using parent hash lookups. 

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.

    // Find the start block height.
    // If no start block is on our chain we start with block 0.
    // TODO: we could return error or empty and drop peer for missing genesis.
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

    // The begin block requested is always one after the start block.
    auto begin = safe_add(start, size_t(1));

    // The maximum number of headers returned is 2000.
    auto end = safe_add(begin, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not confirmed we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash());

        // Otherwise limit the end height to the stop block height.
        // If end precedes begin floor_subtract will handle below.
        if (result)
            end = std::min(result.height(), end);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not confirmed we ignore it.
        const auto result = database_.blocks().get(threshold);

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

////// This may generally execute 29+ queries.
////// There may be a reorg during this query (odd but ok behavior).
////// TODO: generate against any header branch using a header pool branch.
////void block_chain::fetch_block_locator(const block::indexes& heights,
////    block_locator_fetch_handler handler) const
////{
////    if (stopped())
////    {
////        handler(error::service_stopped, nullptr);
////        return;
////    }
////
////    auto message = std::make_shared<get_blocks>();
////    auto& hashes = message->start_hashes();
////    hashes.reserve(heights.size());
////
////    for (const auto height: heights)
////    {
////        // Block locators is generated for the header chain.
////        const auto result = database_.blocks().get(height, false);
////
////        if (!result)
////        {
////            handler(error::not_found, nullptr);
////            break;
////        }
////
////        hashes.push_back(result.hash());
////    }
////
////    handler(error::success, message);
////}

// This may generally execute 29+ queries.
// There may be a reorg during this query (odd but ok behavior).
// TODO: generate against any header branch using a header pool branch.
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
        // Header locators is generated for the header chain.
        const auto result = database_.blocks().get(height, false);

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
// Confirmed heights only.

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

inline bool is_needed(uint8_t block_state)
{
    return block_state == block_state::missing || is_pooled(block_state);
}

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

    // Filter through header pool first (faster than store filter).
    // Excludes blocks that are known to block memory pool (not block pool).
    header_organizer_.filter(message);

    auto& inventories = message->inventories();
    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_block_type() && is_needed(get_block_state(it->hash())))
        {
            ++it;
        }
        else
        {
            it = inventories.erase(it);
        }
    }

    handler(error::success);
}

inline bool is_needed(transaction_state tx_state)
{
    return tx_state == transaction_state::missing ||
        tx_state == transaction_state::pooled;
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

    // Filter through transaction pool first (faster than store filter).
    // Excludes tx that are known to the tx memory pool (not tx pool).
    transaction_organizer_.filter(message);

    auto& inventories = message->inventories();
    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_transaction_type() &&
            get_transaction_state(it->hash()) == transaction_state::missing)
        {
            ++it;
        }
        else
        {
            it = inventories.erase(it);
        }
    }

    handler(error::success);
}

// Subscribers.
//-----------------------------------------------------------------------------

void block_chain::subscribe_headers(reindex_handler&& handler)
{
    // Pass this through to the organizer, which issues the notifications.
    header_organizer_.subscribe(std::move(handler));
}

void block_chain::subscribe_blockchain(reorganize_handler&& handler)
{
    // Pass this through to the organizer, which issues the notifications.
    ////block_organizer_.subscribe(std::move(handler));
}

void block_chain::subscribe_transaction(transaction_handler&& handler)
{
    // Pass this through to the tx organizer, which issues the notifications.
    transaction_organizer_.subscribe(std::move(handler));
}

void block_chain::unsubscribe()
{
    ////block_organizer_.unsubscribe();
    header_organizer_.unsubscribe();
    transaction_organizer_.unsubscribe();
}

// Organizer/Writers.
//-----------------------------------------------------------------------------

void block_chain::organize(header_const_ptr header, result_handler handler)
{
    // The handler must not call organize (lock safety).
    header_organizer_.organize(header, handler);
}

void block_chain::organize(block_const_ptr block, result_handler handler)
{
    // TODO: implement block organize.
    // The handler must not call organize (lock safety).
    ////block_organizer_.organize(block, handler);
}

void block_chain::organize(transaction_const_ptr tx, result_handler handler)
{
    // The handler must not call organize (lock safety).
    transaction_organizer_.organize(tx, handler);
}

void block_chain::update(block_const_ptr block, size_t height,
    result_handler handler)
{
    // TODO: set:
    // block->validation.end_push = asio::steady_clock::now();
    database_.update(block, height, dispatch_, handler);
}

// Properties.
// ----------------------------------------------------------------------------

// non-interface
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
