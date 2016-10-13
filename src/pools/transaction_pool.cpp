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
#include <bitcoin/blockchain/pools/transaction_pool.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <system_error>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

#define NAME "mempool"

using namespace bc::chain;
using namespace bc::message;
using namespace bc::wallet;
using namespace std::placeholders;

// Database access is limited to: index->fetch_history.

transaction_pool::transaction_pool(threadpool& pool, safe_chain& chain,
    const settings& settings)
  : stopped_(true),
    maintain_consistency_(settings.transaction_pool_consistency),
    buffer_(settings.transaction_pool_capacity),
    safe_chain_(chain),
    index_(pool, chain),
    ////validator_(pool, chain, settings),
    subscriber_(std::make_shared<transaction_subscriber>(pool, NAME)),
    dispatch_(pool, NAME)
{
}

void transaction_pool::start()
{
    stopped_ = false;
    index_.start();
    subscriber_->start();

    // Subscribe to blockchain (orphan_pool_manager) reorg notifications.
    safe_chain_.subscribe_reorganize(
        std::bind(&transaction_pool::handle_reorganized,
            this, _1, _2, _3, _4));
}

// The subscriber is not restartable.
void transaction_pool::stop()
{
    stopped_ = true;
    index_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, {}, {});
    clear(error::service_stopped);
}

bool transaction_pool::stopped() const
{
    return stopped_;
}

void transaction_pool::fetch_inventory(size_t limit,
    fetch_inventory_handler handler) const
{
    dispatch_.unordered(&transaction_pool::do_fetch_inventory,
        this, limit, handler);
}

// Populate one *or more* inventory vectors from the full memory pool.
// This is unusual in that the handler may be invoked multiple times.
void transaction_pool::do_fetch_inventory(size_t limit,
    fetch_inventory_handler handler) const
{
    if (buffer_.empty())
    {
        handler(error::success, std::make_shared<inventory>());
        return;
    }

    const auto map = [](const transaction_const_ptr& tx) -> inventory_vector
    {
        return { inventory_vector::type_id::transaction, tx->hash() };
    };

    auto size = buffer_.size();
    auto start_tx = buffer_.begin();

    while (size != 0)
    {
        // Addition is bounded.
        BITCOIN_ASSERT(start_tx != buffer_.end());
        const auto result = std::make_shared<inventory>();
        const auto batch_size = std::min(size, limit);
        auto& inventories = result->inventories();
        inventories.reserve(batch_size);
        auto end_tx = start_tx + batch_size;
        std::transform(start_tx, end_tx, inventories.begin(), map);
        size -= batch_size;
        start_tx = end_tx;

        handler(error::success, result);
    }
}

void transaction_pool::validate(transaction_const_ptr tx,
    validate_handler handler) const
{
    dispatch_.ordered(&transaction_pool::do_validate,
        this, tx, handler);
}

void transaction_pool::do_validate(transaction_const_ptr tx,
    validate_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::operation_failed, {});

    ////validator_->validate(tx,
    ////    std::bind(&transaction_pool::handle_validated,
    ////        this, _1, _2, tx, validator, handler));
}

void transaction_pool::handle_validated(const code& ec,
    const indexes& unconfirmed, transaction_const_ptr tx,
    validate_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    if (ec == error::input_not_found || ec == error::validate_inputs_failed)
    {
        BITCOIN_ASSERT(unconfirmed.size() == 1);
        handler(ec, unconfirmed);
        return;
    }

    if (ec)
    {
        BITCOIN_ASSERT(unconfirmed.empty());
        handler(ec, {});
        return;
    }

    handler(error::success, unconfirmed);
}

// handle_confirm will never fire if handle_validate returns a failure code.
void transaction_pool::organize(transaction_const_ptr tx,
    result_handler handle_confirm, validate_handler handle_validate)
{
    if (stopped())
    {
        handle_validate(error::service_stopped, {});
        return;
    }

    validate(tx,
        std::bind(&transaction_pool::do_organize,
            this, _1, _2, tx, handle_confirm, handle_validate));
}

// This is overly complex due to the transaction pool and index split.
void transaction_pool::do_organize(const code& ec, const indexes& unconfirmed,
    transaction_const_ptr tx, result_handler handle_confirm,
    validate_handler handle_validate)
{
    if (ec)
    {
        handle_validate(ec, {});
        return;
    }

    // Recheck for existence under lock, as a duplicate may have been added.
    if (is_in_pool(tx->hash()))
    {
        handle_validate(error::duplicate, {});
        return;
    }

    // Set up deindexing to run after transaction pool removal.
    const auto do_deindex = [this, tx, handle_confirm](code ec)
    {
        const auto do_confirm = [handle_confirm, ec](code)
        {
            handle_confirm(ec);
        };

        // This always sets success but we have captured the confirmation code.
        index_.remove(tx, do_confirm);
    };

    // Add to pool, save confirmation handler.
    add(tx, do_deindex);

    const auto handle_indexed = [this, handle_validate, tx, unconfirmed](
        code ec)
    {
        // Notify subscribers that the tx has been validated and indexed.
        notify_transaction(unconfirmed, tx);

        LOG_DEBUG(LOG_BLOCKCHAIN)
            << "Transaction saved to mempool (" << buffer_.size() << ")";

        // Notify caller that the tx has been validated and indexed.
        handle_validate(ec, unconfirmed);
    };

    // Add to index and invoke handler to indicate validation and indexing.
    index_.add(tx, handle_indexed);
}

void transaction_pool::fetch(const hash_digest& transaction_hash,
    fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto tx_fetcher = [this, transaction_hash, handler]()
    {
        const auto it = find_iterator(transaction_hash);

        if (it == buffer_.end())
            handler(error::not_found, nullptr);
        else
            handler(error::success, *it);
    };

    dispatch_.ordered(tx_fetcher);
}

void transaction_pool::fetch_history(const payment_address& address,
    size_t limit, size_t from_height,
    safe_chain::history_fetch_handler handler) const
{
    // This passes through to blockchain to build combined history.
    index_.fetch_all_history(address, limit, from_height, handler);
}

// TODO: use hash table pool to eliminate this O(n^2) search.
void transaction_pool::filter(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    const auto filter_transactions = [this, message, handler]()
    {
        auto& inventories = message->inventories();

        for (auto it = inventories.begin(); it != inventories.end();)
        {
            if (it->is_transaction_type() && is_in_pool(it->hash()))
                it = inventories.erase(it);
            else
                ++it;
        }

        handler(error::success);
    };

    dispatch_.ordered(filter_transactions);
}

void transaction_pool::exists(const hash_digest& tx_hash,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    const auto get_existence = [this, tx_hash, handler]()
    {
        handler(is_in_pool(tx_hash) ? error::success : error::not_found);
    };

    dispatch_.ordered(get_existence);
}

// new blocks come in - remove txs in new
// old blocks taken out - resubmit txs in old
bool transaction_pool::handle_reorganized(const code& ec, size_t fork_point,
    const block_const_ptr_list& new_blocks,
    const block_const_ptr_list& replaced_blocks)
{
    if (ec == error::service_stopped)
    {
        LOG_DEBUG(LOG_BLOCKCHAIN)
            << "Stopping transaction pool: " << ec.message();
        return false;
    }

    if (ec)
    {
        LOG_DEBUG(LOG_BLOCKCHAIN)
            << "Failure in tx pool reorganize handler: " << ec.message();
        return false;
    }

    LOG_DEBUG(LOG_BLOCKCHAIN)
        << "Reorganize: tx pool size (" << buffer_.size()
        << ") forked at (" << fork_point
        << ") new blocks (" << new_blocks.size()
        << ") replace blocks (" << replaced_blocks.size() << ")";

    if (replaced_blocks.empty())
    {
        // Remove memory pool transactions that also exist in new blocks.
        dispatch_.ordered(
            std::bind(&transaction_pool::remove,
                this, new_blocks));
    }
    else
    {
        // See www.jwz.org/doc/worse-is-better.html
        // for why we take this approach. We return with an error_code.
        // An alternative would be resubmit all tx from the cleared blocks.
        dispatch_.ordered(
            std::bind(&transaction_pool::clear,
                this, error::blockchain_reorganized));
    }

    return true;
}

void transaction_pool::subscribe_transaction(
    transaction_handler handle_transaction)
{
    subscriber_->subscribe(handle_transaction, error::service_stopped, {},
        nullptr);
}

void transaction_pool::notify_transaction(const point::indexes& unconfirmed,
    transaction_const_ptr tx)
{
    subscriber_->relay(error::success, unconfirmed, tx);
}

// Entry methods.
// ----------------------------------------------------------------------------

// A new transaction has been received, add it to the memory pool.
void transaction_pool::add(transaction_const_ptr tx, result_handler handler)
{
    // When a new tx is added to the buffer drop the oldest.
    if (maintain_consistency_ && buffer_.size() == buffer_.capacity())
        delete_package(error::pool_filled);

    tx->validation.confirm = handler;
    buffer_.push_back(tx);
}

// There has been a reorg, clear the memory pool using the given reason code.
void transaction_pool::clear(const code& ec)
{
    for (const auto tx: buffer_)
        if (tx->validation.confirm != nullptr)
            tx->validation.confirm(ec);

    buffer_.clear();
}

// Delete memory pool txs that are obsoleted by a new block acceptance.
void transaction_pool::remove(const block_const_ptr_list& blocks)
{
    // Delete by hash sets a success code.
    delete_confirmed_in_blocks(blocks);

    // Delete by spent sets a double-spend error.
    if (maintain_consistency_)
        delete_spent_in_blocks(blocks);
}

// Consistency methods.
// ----------------------------------------------------------------------------

// Delete mempool txs that are duplicated in the new blocks.
void transaction_pool::delete_confirmed_in_blocks(
    const block_const_ptr_list& blocks)
{
    if (stopped() || buffer_.empty())
        return;

    for (const auto block: blocks)
        for (const auto& tx: block->transactions())
            delete_single(tx.hash(), error::success);
}

// Delete all txs that spend a previous output of any tx in the new blocks.
void transaction_pool::delete_spent_in_blocks(
    const block_const_ptr_list& blocks)
{
    if (stopped() || buffer_.empty())
        return;

    for (const auto block: blocks)
        for (const auto& tx: block->transactions())
            for (const auto& input: tx.inputs())
                delete_dependencies(input.previous_output(),
                    error::double_spend);
}

// Delete any tx that spends any output of this tx.
void transaction_pool::delete_dependencies(const output_point& point,
    const code& ec)
{
    const auto comparitor = [&point](const input& input)
    {
        return input.previous_output() == point;
    };

    delete_dependencies(comparitor, ec);
}

// Delete any tx that spends any output of this tx.
void transaction_pool::delete_dependencies(const hash_digest& tx_hash,
    const code& ec)
{
    const auto comparitor = [&tx_hash](const input& input)
    {
        return input.previous_output().hash() == tx_hash;
    };

    delete_dependencies(comparitor, ec);
}

// This is horribly inefficient, but it's simple.
// TODO: Create persistent multi-indexed memory pool (including age and
// children) and perform this pruning trivialy (and add policy over it).
void transaction_pool::delete_dependencies(input_compare is_dependency,
    const code& ec)
{
    transaction_const_ptr_list dependencies;

    for (const auto tx: buffer_)
        for (const auto& input: tx->inputs())
            if (is_dependency(input))
            {
                dependencies.push_back(tx);
                break;
            }

    // We queue deletion to protect the iterator.
    for (const auto tx: dependencies)
        delete_package(tx, ec);
}

void transaction_pool::delete_package(const code& ec)
{
    if (stopped() || buffer_.empty())
        return;

    // Must copy the entry because it is going to be deleted from the list.
    const auto oldest_tx = buffer_.front();

    if (oldest_tx->validation.confirm != nullptr)
        oldest_tx->validation.confirm(ec);

    delete_package(oldest_tx, ec);
}

void transaction_pool::delete_package(transaction_const_ptr tx, const code& ec)
{
    if (delete_single(tx->hash(), ec))
        delete_dependencies(tx->hash(), ec);
}

bool transaction_pool::delete_single(const hash_digest& tx_hash, const code& ec)
{
    if (stopped())
        return false;

    const auto matched = [&tx_hash](const transaction_const_ptr& tx)
    {
        return tx->hash() == tx_hash;
    };

    const auto it = std::find_if(buffer_.begin(), buffer_.end(), matched);

    if (it == buffer_.end())
        return false;

    const auto confirmation_callback = (*it)->validation.confirm;

    if (confirmation_callback != nullptr)
        confirmation_callback(ec);

    buffer_.erase(it);
    return true;
}

transaction_const_ptr transaction_pool::find(const hash_digest& tx_hash) const
{
    const auto it = find_iterator(tx_hash);
    const auto found = it != buffer_.end();
    return found ? *it : nullptr;
}

transaction_pool::const_iterator transaction_pool::find_iterator(
    const hash_digest& tx_hash) const
{
    const auto found = [&tx_hash](const transaction_const_ptr& tx)
    {
        return tx->hash() == tx_hash;
    };

    return std::find_if(buffer_.begin(), buffer_.end(), found);
}

bool transaction_pool::is_in_pool(const hash_digest& tx_hash) const
{
    return find_iterator(tx_hash) != buffer_.end();
}

bool transaction_pool::is_spent_in_pool(transaction_const_ptr tx) const
{
    const auto found = [this](const input& input)
    {
        return is_spent_in_pool(input.previous_output());
    };

    const auto& inputs = tx->inputs();
    return std::any_of(inputs.begin(), inputs.end(), found);
}

bool transaction_pool::is_spent_in_pool(const output_point& outpoint) const
{
    const auto found = [this, &outpoint](const transaction_const_ptr& tx)
    {
        return is_spent_by_tx(outpoint, tx);
    };

    return std::any_of(buffer_.begin(), buffer_.end(), found);
}

// static
bool transaction_pool::is_spent_by_tx(const output_point& outpoint,
    transaction_const_ptr tx)
{
    const auto found = [&outpoint](const input& input)
    {
        return input.previous_output() == outpoint;
    };

    const auto& inputs = tx->inputs();
    return std::any_of(inputs.begin(), inputs.end(), found);
}

} // namespace blockchain
} // namespace libbitcoin
