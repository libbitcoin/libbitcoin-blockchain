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
    fork_point_({ null_hash, 0 }),
    database_(database_settings),
    chain_state_populator_(*this, chain_settings),
    index_addresses_(database_settings.index_addresses),

    // Enable block/header priority when write flush enabled (performance).
    validation_mutex_(database_settings.flush_writes),

    // One dedicated thread is required by the validation subscriber.
    priority_pool_(thread_ceiling(chain_settings.cores) + 1u, priority(chain_settings.priority)),
    dispatch_(priority_pool_, NAME "_priority"),

    // TODO: review organizer use of mutex and priority thread pool.
    block_organizer_(validation_mutex_, dispatch_, priority_pool_, *this, chain_settings),
    header_organizer_(validation_mutex_, dispatch_, pool, *this, chain_settings),
    transaction_organizer_(validation_mutex_, dispatch_, pool, *this, chain_settings),

    // TODO: review organizer use of mutex and priority thread pool.
    block_subscriber_(std::make_shared<block_subscriber>(pool, NAME "_block")),
    header_subscriber_(std::make_shared<header_subscriber>(pool, NAME "_header")),
    transaction_subscriber_(std::make_shared<transaction_subscriber>(pool, NAME "_transaction"))
{
}

// ============================================================================
// FAST CHAIN
// ============================================================================

// Readers.
// ----------------------------------------------------------------------------

bool block_chain::get_if_empty(hash_digest& out_hash, size_t height) const
{
    const auto result = database_.blocks().get(height, false);

    // Do not download if not found, invalid or already populated.
    if (!result || is_failed(result.state()) ||
        result.transaction_count() != 0)
        return false;

    out_hash = result.hash();
    return true;
}

bool block_chain::get_top(chain::header& out_header, size_t& out_height,
    bool block_index) const
{
    return get_top_height(out_height, block_index) &&
        get_header(out_header, out_height, block_index);
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

bool block_chain::get_header(chain::header& out_header, size_t height,
    bool block_index) const
{
    auto result = database_.blocks().get(height, block_index);

    if (!result)
        return false;

    // Since header() is const this may not actually move without a cast.
    out_header = std::move(result.header());
    return true;
}

bool block_chain::get_header(chain::header& out_header, size_t& out_height,
    const hash_digest& block_hash, bool block_index) const
{
    auto result = database_.blocks().get(block_hash);

    if (!result)
        return false;

    const auto height = result.height();

    // The only way to know if a header is indexed is from its presence in the
    // index. It will not be marked as a candidate until validated as such.
    if (database_.blocks().get(height, block_index))
    {
        out_header = result.header();
        out_height = height;
        return true;
    }

    return false;
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

bool block_chain::get_work(uint256_t& out_work, const uint256_t& overcome,
    size_t above_height, bool block_index) const
{
    size_t top;
    out_work = 0;

    if (!database_.blocks().top(top, block_index))
        return false;

    // Set overcome to zero to bypass early exit.
    const auto no_maximum = overcome.is_zero();

    for (auto height = top; (height > above_height) && 
        (no_maximum || out_work <= overcome); --height)
    {
        const auto result = database_.blocks().get(height, block_index);

        if (!result)
            return false;

        // Candidate chain is counted only to top validated block.
        if (!block_index && !is_valid(result.state()))
            break;

        out_work += chain::header::proof(result.bits());
    }

    return true;
}

// TODO: move into block database (see populate_output/get_output below).
void block_chain::populate_header(const chain::header& header) const
{
    const auto result = database_.blocks().get(header.hash());

    // Default values presumed correct for indication of not found.
    if (!result)
        return;

    const auto state = result.state();
    const auto height = result.height();

    header.metadata.error = result.error();
    header.metadata.exists = true;
    header.metadata.populated = result.transaction_count() != 0;
    header.metadata.validated = is_valid(state) || is_failed(state);
    header.metadata.candidate = is_candidate(state);
    header.metadata.confirmed = is_confirmed(state);
}

// TODO: move into tx database (see populate_output/get_output below).
void block_chain::populate_block_transaction(const chain::transaction& tx,
    uint32_t forks, size_t fork_height) const
{
    const auto result = database_.transactions().get(tx.hash());

    // Default values are correct for indication of not found.
    if (!result)
        return;

    const auto bip34 = chain::script::is_enabled(forks,
        machine::rule_fork::bip34_rule);

    //*************************************************************************
    // CONSENSUS: BIP30 treats a spent duplicate as if it did not exist, and
    // any duplicate of an unspent tx as invalid (with two expcetions).
    // BIP34 active renders BIP30 moot as duplicates are presumed impossible.
    //*************************************************************************
    if (!bip34 && result.is_spent(fork_height, true))
    {
        // The original tx will not be queryable independent of the block.
        // The original tx's block linkage is unbroken by accepting duplicate.
        // BIP30 exception blocks are not spent (will not be unlinked here).
        BITCOIN_ASSERT(tx.metadata.link == transaction::validation::unlinked);
        return;
    }

    const auto state = result.state();
    const auto height = result.height();
    const auto relevant = fork_height <= height;
    tx.metadata.link = result.link();
    tx.metadata.existed = tx.metadata.link == transaction::validation::unlinked;
    tx.metadata.candidate = state == transaction_state::candidate;
    tx.metadata.confirmed = state == transaction_state::confirmed && relevant;
    tx.metadata.verified = state != transaction_state::confirmed &&
        height == forks;
}

// TODO: move into tx database (see populate_output/get_output below).
void block_chain::populate_pool_transaction(const chain::transaction& tx,
    uint32_t forks) const
{
    const auto result = database_.transactions().get(tx.hash());

    // Default values presumed correct for indication of not found.
    if (!result)
        return;

    const auto state = result.state();
    const auto height = result.height();
    tx.metadata.link = result.link();
    tx.metadata.existed = tx.metadata.link == transaction::validation::unlinked;
    tx.metadata.candidate = state == transaction_state::candidate;
    tx.metadata.confirmed = state == transaction_state::confirmed;
    tx.metadata.verified = state != transaction_state::confirmed &&
        height == forks;
}

void block_chain::populate_output(const chain::output_point& outpoint,
    size_t fork_height, bool candidate) const
{
    database_.transactions().get_output(outpoint, fork_height, candidate);
}

uint8_t block_chain::get_block_state(size_t height, bool block_index) const
{
    return database_.blocks().get(height, block_index).state();
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

block_const_ptr block_chain::get_block(size_t height, bool witness,
    bool block_index) const
{
    // TODO: not currently populated.
    const auto cached = last_block_.load();

    // Try the cached block first.
    if (block_index && cached && cached->header().metadata.state &&
        cached->header().metadata.state->height() == height)
        return cached;

    const auto result = database_.blocks().get(height, block_index);

    // A populated block was not found at the given height.
    if (!result || result.transaction_count() == 0)
        return {};

    transaction::list txs;
    BITCOIN_ASSERT(result.height() == height);

    // False implies store corruption.
    DEBUG_ONLY(const auto value =) get_transactions(txs, result, witness);
    BITCOIN_ASSERT(value);
    return std::make_shared<const block>(result.header(), std::move(txs));
}

header_const_ptr block_chain::get_header(size_t height, bool block_index) const
{
    const auto result = database_.blocks().get(height, block_index);

    // A header was not found at the given height.
    if (!result)
        return {};

    BITCOIN_ASSERT(result.height() == height);
    return std::make_shared<const header>(result.header());
}

// Writers
// ----------------------------------------------------------------------------

// private
void block_chain::index_block(block_const_ptr block)
{
    if (!index_addresses_)
        return;

    code ec;
    if ((ec = database_.index(*block)))
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure in block payment indexing, store is now corrupt: "
            << ec.message();

        // In the case of a store failure the server stops processing.
        stop();
    }
}

// private
void block_chain::index_transaction(transaction_const_ptr tx)
{
    if (!index_addresses_ || tx->metadata.existed)
        return;

    code ec;
    if ((ec = database_.index(*tx)))
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure in transaction payment indexing, store is now corrupt: "
            << ec.message();

        // In the case of a store failure the server stops processing.
        stop();
    }
}

code block_chain::store(transaction_const_ptr tx)
{
    const auto state = tx->metadata.state;

    if (!state)
        return error::operation_failed;

    last_transaction_.store(tx);

    ////// Clear metadata state (but need on last_transaction_).
    ////tx->metadata.state.reset();

    // Payment indexing is asynchronous, after tx is stored. Therefore
    // it is possible for a tx to be in any existing state and not be indexed.
    if (index_addresses_ && !tx->metadata.existed)
    {
        // TODO: use low priority thread for this.
        dispatch_.concurrent(&block_chain::index_transaction, this, tx);
    }

    code ec;
    if ((ec = database_.store(*tx, state->enabled_forks())))
        return ec;

    notify(tx);
    return ec;
}

// private static
uint256_t block_chain::work(header_const_ptr_list_const_ptr headers)
{
    uint256_t total;

    // Not using accumulator here avoids repeated copying of uint256 object.
    for (auto header: *headers)
        total += header->proof();

    return total;
}

code block_chain::reorganize(const config::checkpoint& fork,
    header_const_ptr_list_const_ptr incoming)
{
    if (incoming->empty())
        return error::operation_failed;

    const auto top = incoming->back();
    const auto top_state = top->metadata.state;
    if (!top_state)
        return error::operation_failed;

    code ec;
    header_const_ptr_list_ptr outgoing;

    ////// Clear metadata.state (but need on stored cache).
    ////for (const auto header: *incoming)
    ////    header->metadata.state.reset();

    // This unmarks candidate txs and spent outputs (may have been validated).
    if ((ec = database_.reorganize(fork, incoming, outgoing)))
        return ec;

    const auto fork_height = fork.height();

    // If confirmed fork point is above candidate fork point then lower it.
    if (fork_point().height() > fork_height)
    {
        // Set new fork point and recompute confirmed work from it.
        set_fork_point(fork);
        set_confirmed_work();

        // When fork point is lowered the top valid candidate is at fork point.
        auto top_valid = chain_state_populator_.populate(fork_height, false);
        set_top_valid_candidate_state(top_valid);
        set_candidate_work(0);
    }

    set_top_candidate_state(top_state);
    notify(fork_height, incoming, outgoing);
    return ec;
}

code block_chain::update(block_const_ptr block, size_t height)
{
    // This stores or connects each transaction and sets tx link metadata.
    return database_.update(*block, height);
}

// Mark candidate block and descendants as invalid and pop them.
code block_chain::invalidate(block_const_ptr block, size_t block_height)
{
    const auto& header = block->header();
    BITCOIN_ASSERT(header.metadata.error);

    size_t top;
    if (!get_top_height(top, false))
        return error::operation_failed;

    hash_digest fork_hash;
    const auto fork_height = block_height - 1u;
    if (!get_block_hash(fork_hash, fork_height, false))
        return error::operation_failed;

    code ec;
    const config::checkpoint fork{ fork_hash, fork_height };
    const auto outgoing = std::make_shared<header_const_ptr_list>();
    const auto incoming = std::make_shared<header_const_ptr_list>();

    // Copy first invalid candidate header to outgoing.
    outgoing->push_back(std::make_shared<const message::header>(header));

    // Copy all dependant invalid candidates to outgoing.
    for (auto height = block_height + 1u; height <= top; ++height)
        outgoing->push_back(get_header(height, false));

    // Mark all outgoing candidate blocks as invalid, in store and metadata.
    for (const auto header: *outgoing)
        if ((ec = database_.invalidate(*header,
            error::store_block_missing_parent)))
            return ec;

    ////// Clear metadata.state (but need on stored cache).
    ////for (const auto header: *incoming)
    ////    header->metadata.state.reset();

    // TODO: check for subscription dependencies on non-empty incoming.
    // This should not have to unmark because none were ever valid.
    if ((ec = database_.reorganize(fork, incoming, outgoing)))
        return ec;

    // Lower top candidate state to that of the top valid (previous header).
    set_top_candidate_state(top_valid_candidate_state());

    notify(fork_height, incoming, outgoing);
    return ec;
}

// Mark candidate block as valid and mark candidate-spent outputs.
code block_chain::candidate(block_const_ptr block)
{
    code ec;
    const auto& header = block->header();
    BITCOIN_ASSERT(!header.metadata.error);

    // Mark candidate block valid, txs and outputs spent by them as candidate.
    if ((ec = database_.candidate(*block)))
        return ec;

    // Advance the top valid candidate state and candidate work.
    set_top_valid_candidate_state(header.metadata.state);
    set_candidate_work(candidate_work() + header.proof());

    // Payment indexing is asynchronous, after block is candidate. Therefore
    // it is possible for a block to be in any valid state and not be indexed.
    if (index_addresses_)
    {
        // TODO: use low priority thread for this.
        dispatch_.concurrent(&block_chain::index_block, this, block);
    }

    return ec;
}

// Reorganize this stronger candidate branch into confirmed chain.
code block_chain::reorganize(block_const_ptr_list_const_ptr branch_cache,
    size_t branch_height)
{
    if (branch_cache->empty())
        return error::operation_failed;

    const auto top = branch_cache->back();
    const auto top_state = top->header().metadata.state;
    if (!top_state)
        return error::operation_failed;

    code ec;
    const auto fork = fork_point();
    const auto outgoing = std::make_shared<block_const_ptr_list>();
    const auto incoming = std::make_shared<block_const_ptr_list>();

    // Get all missing incoming candidates (expensive reads).
    for (auto height = fork.height() + 1u; height < branch_height; ++height)
        incoming->push_back(get_block(height, true, false));

    // Copy all candidate pointers from the branch cache.
    for (const auto block: *branch_cache)
    {
        ////// Clear metadata state (but need on last_block_).
        ////block->header().metadata.state.reset();
        incoming->push_back(block);
    }

    // This unmarks candidate txs and spent outputs (because confirmed).
    if ((ec = database_.reorganize(fork, incoming, outgoing)))
        return ec;

    // Top valid candidate is now top confirmed and the new fork point.
    set_fork_point({ top->hash(), top_state->height() });
    set_candidate_work(0);
    set_confirmed_work(0);
    set_next_confirmed_state(top_state);
    last_block_.store(top);
    notify(fork.height(), incoming, outgoing);
    return ec;
}

// Properties.
// ----------------------------------------------------------------------------

// private.
config::checkpoint block_chain::fork_point() const
{
    return fork_point_.load();
}

// private.
uint256_t block_chain::candidate_work() const
{
    return candidate_work_.load();
}

// private.
uint256_t block_chain::confirmed_work() const
{
    return confirmed_work_.load();
}

chain::chain_state::ptr block_chain::top_candidate_state() const
{
    return top_candidate_state_.load();
}

chain::chain_state::ptr block_chain::top_valid_candidate_state() const
{
    return top_valid_candidate_state_.load();
}

chain::chain_state::ptr block_chain::next_confirmed_state() const
{
    return next_confirmed_state_.load();
}

// private.
bool block_chain::set_fork_point()
{
    size_t candidate_height;
    size_t confirmed_height;

    if (!get_top_height(candidate_height, false) ||
        !get_top_height(confirmed_height, true))
        return false;

    hash_digest candidate_hash;
    hash_digest confirmed_hash;
    auto common = std::min(candidate_height, confirmed_height);

    // The loop must at least terminate on the genesis block.
    BITCOIN_ASSERT(get_block_hash(candidate_hash, 0, false));
    BITCOIN_ASSERT(get_block_hash(confirmed_hash, 0, true));
    BITCOIN_ASSERT(candidate_hash == confirmed_hash);

    while (get_block_hash(candidate_hash, common, false) &&
        get_block_hash(confirmed_hash, common, true) &&
        candidate_hash != confirmed_hash)
        --common;

    set_fork_point({ confirmed_hash, common });
    return true;
}

// private.
bool block_chain::set_candidate_work()
{
    BITCOIN_ASSERT_MSG(fork_point().hash() != null_hash, "Set fork point.");

    uint256_t work_above_fork;
    if (!get_work(work_above_fork, 0, fork_point().height(), false))
        return false;

    set_candidate_work(work_above_fork);
    return true;
}

// private.
bool block_chain::set_confirmed_work()
{
    BITCOIN_ASSERT_MSG(fork_point().hash() != null_hash, "Set fork point.");

    uint256_t work_above_fork;
    if (!get_work(work_above_fork, 0, fork_point().height(), true))
        return false;

    set_confirmed_work(work_above_fork);
    return true;
}

// private.
bool block_chain::set_top_candidate_state()
{
    set_top_candidate_state(chain_state_populator_.populate(false));
    return top_candidate_state() != nullptr;
}

// private.
bool block_chain::set_top_valid_candidate_state()
{
    size_t height;
    if (!get_top_height(height, false))
        return false;

    // The loop must at least terminate on the genesis block.
    BITCOIN_ASSERT(is_valid_candidate(get_block_state(0, false)));

    // Block marked candidate when validated in candidate chain.
    // Block unmarked candidate when leaves candidate chain.
    // Block will be valid and unmarked candidate upon reentry.
    while (!is_valid_candidate(get_block_state(height, false)))
        --height;

    const auto state = chain_state_populator_.populate(height, false);
    set_top_valid_candidate_state(state);
    return top_valid_candidate_state() != nullptr;
}

// private.
bool block_chain::set_next_confirmed_state()
{
    set_next_confirmed_state(chain_state_populator_.populate(true));
    return next_confirmed_state() != nullptr;
}

// private.
void block_chain::set_fork_point(const config::checkpoint& fork)
{
    fork_point_.store(fork);
}

// private.
void block_chain::set_candidate_work(const uint256_t& work_above_fork)
{
    candidate_work_.store(work_above_fork);
}

// private.
void block_chain::set_confirmed_work(const uint256_t& work_above_fork)
{
    confirmed_work_.store(work_above_fork);
}

// private.
void block_chain::set_top_candidate_state(chain::chain_state::ptr top)
{
    top_candidate_state_.store(top);
}

// private.
void block_chain::set_top_valid_candidate_state(chain::chain_state::ptr top)
{
    top_valid_candidate_state_.store(top);
}

// private.
void block_chain::set_next_confirmed_state(chain::chain_state::ptr top)
{
    // Promotion always succeeds.
    // Tx pool state is promoted from the state of the top confirmed block.
    next_confirmed_state_.store(std::make_shared<chain::chain_state>(*top));
}

bool block_chain::is_candidates_stale() const
{
    // The header state is as fresh as the last (top) indexed header.
    const auto state = top_candidate_state_.load();
    return state && state->is_stale();
}

bool block_chain::is_validated_stale() const
{
    // The valid candidate state is as fresh as the top valid candidate.
    const auto state = top_valid_candidate_state_.load();
    return state && state->is_stale();
}

bool block_chain::is_blocks_stale() const
{
    // The pool state is as fresh as the last (top) indexed block.
    const auto state = next_confirmed_state_.load();
    return state && state->is_stale();
}

bool block_chain::is_reorganizable() const
{
    return candidate_work() > confirmed_work();
}

// Chain State
// ----------------------------------------------------------------------------

// For header index validator, call only from validate critical section.
chain::chain_state::ptr block_chain::chain_state(const chain::header& header,
    size_t height) const
{
    return chain_state_populator_.populate(header, height, false);
}

// Promote chain state from the given parent header.
chain::chain_state::ptr block_chain::promote_state(const chain::header& header,
    chain::chain_state::ptr parent) const
{
    if (!parent || parent->hash() !=header.previous_block_hash())
        return {};

    return std::make_shared<chain::chain_state>(*parent, header);
}

// Promote chain state for the last block in the multi-header branch.
chain::chain_state::ptr block_chain::promote_state(
    header_branch::const_ptr branch) const
{
    const auto parent = branch->top_parent();
    if (!parent || !parent->metadata.state)
        return {};

    return promote_state(*branch->top(), parent->metadata.state);
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

    block_subscriber_->start();
    header_subscriber_->start();
    transaction_subscriber_->start();

    return set_fork_point()
        && set_top_candidate_state()
        && set_top_valid_candidate_state()
        && set_next_confirmed_state()
        && set_candidate_work()
        && set_confirmed_work()
        && block_organizer_.start()
        && header_organizer_.start()
        && transaction_organizer_.start();
}

bool block_chain::stop()
{
    stopped_ = true;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    validation_mutex_.lock_high_priority();

    // This cannot call organize or stop (lock safe).
    auto result =
        block_organizer_.stop() &&
        header_organizer_.stop() &&
        transaction_organizer_.stop();

    block_subscriber_->stop();
    header_subscriber_->stop();
    transaction_subscriber_->stop();

    block_subscriber_->invoke(error::service_stopped, 0, {}, {});
    header_subscriber_->invoke(error::service_stopped, 0, {}, {});
    transaction_subscriber_->invoke(error::service_stopped, {});

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
    const database::block_result& result, bool witness) const
{
    out_transactions.reserve(result.transaction_count());
    const auto& tx_store = database_.transactions();

    for (const auto offset: result)
    {
        const auto result = tx_store.get(offset);

        if (!result)
            return false;

        out_transactions.push_back(result.transaction(witness));
    }

    return true;
}

// private
bool block_chain::get_transaction_hashes(hash_list& out_hashes,
    const database::block_result& result) const
{
    out_hashes.reserve(result.transaction_count());
    const auto& tx_store = database_.transactions();

    for (const auto offset: result)
    {
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
    if (cached && cached->header().metadata.state &&
        cached->header().metadata.state->height() == height)
    {
        handler(error::success, cached, height);
        return;
    }

    const auto result = database_.blocks().get(height);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    transaction::list txs;
    BITCOIN_ASSERT(result.height() == height);

    if (!get_transactions(txs, result, witness))
    {
        handler(error::operation_failed, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<const block>(result.header(),
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
    if (cached && cached->header().metadata.state && cached->hash() == hash)
    {
        const auto height = cached->header().metadata.state->height();
        handler(error::success, cached, height);
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    transaction::list txs;

    if (!get_transactions(txs, result, witness))
    {
        handler(error::operation_failed, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<const block>(result.header(),
        std::move(txs));
    handler(error::success, message, result.height());
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

    if (!get_transaction_hashes(hashes, result))
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

    if (!get_transaction_hashes(hashes, result))
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

        if (cached && cached->metadata.state && cached->hash() == hash)
        {
            ////LOG_INFO(LOG_BLOCKCHAIN) << "TX CACHE HIT";

            // Simulate the position and height overloading of the database.
            const auto height = cached->metadata.state->height();
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

        if (cached && cached->metadata.state && cached->hash() == hash)
        {
            ////LOG_INFO(LOG_BLOCKCHAIN) << "TX CACHE HIT";

            // Simulate the position and height overloading of the database.
            const auto height = cached->metadata.state->height();
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

// TODO: reimplement in store.
void block_chain::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    ////auto point = database_.spends().get(outpoint);

    ////if (point.hash() == null_hash)
    ////{
    ////    handler(error::not_found, {});
    ////    return;
    ////}

    ////handler(error::success, std::move(point));

    handler(error::not_implemented, {});
}

// TODO: could return an iterator with an internal tx store reference, which
// would eliminate problem of excessive memory allocation, however the
// allocation lock cost may be problematic.
void block_chain::fetch_history(const short_hash& address_hash, size_t limit,
    size_t from_height, history_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    // Cannot know size without reading all, so dynamically allocate.
    chain::payment_record::list payments;
    size_t count = 0;

    // Result set is ordered most recent tx first (reverse point order in tx).
    for (auto payment: database_.addresses().get(address_hash))
    {
        if (count++ == limit)
            break;

        const auto tx = database_.transactions().get(payment.link());
        const auto height = tx.height();

        if (height < from_height)
            break;

        // Copy of each payment record above so can be modified here.
        payment.set_height(height);
        payment.set_hash(tx.hash());
        payments.push_back(payment);
    }

    handler(error::success, std::move(payments));
}

// TODO: eliminate.
void block_chain::fetch_stealth(const binary&, size_t,
    stealth_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::not_implemented, {});
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

    // Filter through header pool first (faster than store filter).
    // Excludes blocks that are known to block memory pool (not block pool).
    header_organizer_.filter(message);

    auto& inventories = message->inventories();
    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_block_type() && !database_.blocks().get(it->hash()))
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
    return tx_state == transaction_state::pooled;
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
            !database_.transactions().get(it->hash()))
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

void block_chain::subscribe(block_handler&& handler)
{
    block_subscriber_->subscribe(std::move(handler),
        error::service_stopped, 0, {}, {});
}

void block_chain::subscribe(header_handler&& handler)
{
    header_subscriber_->subscribe(std::move(handler),
        error::service_stopped, 0, {}, {});
}

void block_chain::subscribe(transaction_handler&& handler)
{
    transaction_subscriber_->subscribe(std::move(handler),
        error::service_stopped, {});
}

void block_chain::unsubscribe()
{
    block_subscriber_->relay(error::success, 0, {}, {});
    header_subscriber_->relay(error::success, 0, {}, {});
    transaction_subscriber_->relay(error::success, {});
}

// protected
void block_chain::notify(size_t fork_height,
    block_const_ptr_list_const_ptr incoming,
    block_const_ptr_list_const_ptr outgoing)
{
    // This invokes handlers within the criticial section (deadlock risk).
    block_subscriber_->invoke(error::success, fork_height, incoming, outgoing);
}

// protected
void block_chain::notify(size_t fork_height,
    header_const_ptr_list_const_ptr incoming,
    header_const_ptr_list_const_ptr outgoing)
{
    // This invokes handlers within the criticial section (deadlock risk).
    header_subscriber_->invoke(error::success, fork_height, incoming, outgoing);
}

// protected
void block_chain::notify(transaction_const_ptr tx)
{
    // This invokes handlers within the criticial section (deadlock risk).
    transaction_subscriber_->invoke(error::success, tx);
}

// Organizer/Writers.
//-----------------------------------------------------------------------------

void block_chain::organize(header_const_ptr header, result_handler handler)
{
    // The handler must not call organize (lock safety).
    header_organizer_.organize(header, handler);
}

void block_chain::organize(transaction_const_ptr tx, result_handler handler)
{
    // The handler must not call organize (lock safety).
    transaction_organizer_.organize(tx, handler);
}

code block_chain::organize(block_const_ptr block, size_t height)
{
    // This triggers block and header reorganization notifications.
    return block_organizer_.organize(block, height);
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
