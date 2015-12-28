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
#include <bitcoin/blockchain/transaction_pool.hpp>

#include <algorithm>
#include <cstddef>
#include <system_error>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/blockchain.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>
#include <bitcoin/blockchain/validate_transaction.hpp>

namespace libbitcoin {
namespace chain {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

transaction_pool::transaction_pool(threadpool& pool, blockchain& chain,
    size_t capacity, bool consistency)
  : strand_(pool),
    blockchain_(chain),
    buffer_(capacity),
    stopped_(true),
    maintain_consistency_(consistency)
{
}

transaction_pool::~transaction_pool()
{
    clear(error::service_stopped);
}

bool transaction_pool::empty() const
{
    return buffer_.empty();
}

size_t transaction_pool::size() const
{
    return buffer_.size();
}

bool transaction_pool::start()
{
    // TODO: can we actually restart?
    stopped_ = false;

    // Subscribe to blockchain (organizer) reorg notifications.
    blockchain_.subscribe_reorganize(
        std::bind(&transaction_pool::reorganize,
            this, _1, _2, _3, _4));

    return true;
}

bool transaction_pool::stop()
{
    // Stop doesn't need to be called externally and could be made private.
    // This will arise from a reorg shutdown message, so transaction_pool
    // is automatically registered for shutdown in the following sequence.
    // blockchain->organizer(orphan/block pool)->transaction_pool
    stopped_ = true;
    return true;
}

bool transaction_pool::stopped()
{
    return stopped_;
}

void transaction_pool::validate(const transaction_type& tx,
    validate_handler handle_validate)
{
    strand_.queue(&transaction_pool::do_validate,
        this, tx, handle_validate);
}

void transaction_pool::do_validate(const transaction_type& tx,
    validate_handler handle_validate)
{
    if (stopped())
    {
        handle_validate(error::service_stopped, index_list());
        return;
    }

    // This must be allocated as a shared pointer reference in order for
    // validate_transaction::start to create a second reference.
    const auto validate = std::make_shared<validate_transaction>(
        blockchain_, tx, buffer_, strand_);

    validate->start(
        strand_.wrap(&transaction_pool::validation_complete,
            this, _1, _2, hash_transaction(tx), handle_validate));
}

void transaction_pool::validation_complete(
    const std::error_code& ec, const index_list& unconfirmed,
    const hash_digest& tx_hash, validate_handler handle_validate)
{
    if (stopped())
    {
        handle_validate(error::service_stopped, index_list());
        return;
    }

    if (ec == error::input_not_found || ec == error::validate_inputs_failed)
    {
        BITCOIN_ASSERT(unconfirmed.size() == 1);
        //BITCOIN_ASSERT(unconfirmed[0] < tx.inputs.size());
        handle_validate(ec, unconfirmed);
        return;
    }

    // We don't stop for a validation error.
    if (ec)
    {
        BITCOIN_ASSERT(unconfirmed.empty());
        handle_validate(ec, index_list());
        return;
    }

    // Re-check as another transaction might have been added in the interim.
    if (tx_exists(tx_hash))
        handle_validate(error::duplicate, index_list());
    else
        handle_validate(error::success, unconfirmed);
}

bool transaction_pool::tx_exists(const hash_digest& hash)
{
    return tx_find(hash) != buffer_.end();
}

pool_buffer::const_iterator transaction_pool::tx_find(const hash_digest& hash)
{
    const auto found = [&hash](const transaction_entry_info& entry)
    {
        return entry.hash == hash;
    };

    return std::find_if(buffer_.begin(), buffer_.end(), found);
}

// handle_confirm will never fire if handle_validate returns a failure code.
void transaction_pool::store(const transaction_type& tx,
    confirm_handler handle_confirm, validate_handler handle_validate)
{
    if (stopped())
    {
        handle_validate(error::service_stopped, index_list());
        return;
    }

    const auto wrap_validate = [this, tx, handle_confirm, handle_validate]
        (const std::error_code& ec, const index_list& unconfirmed)
    {
        if (!ec)
        {
            add(tx, handle_confirm);

            log_debug(LOG_BLOCKCHAIN)
                << "Transaction saved to mempool (" << buffer_.size() << ")";
        }

        handle_validate(ec, unconfirmed);
    };

    validate(tx, wrap_validate);
}

void transaction_pool::fetch(const hash_digest& transaction_hash,
    fetch_handler handle_fetch)
{
    if (stopped())
    {
        handle_fetch(error::service_stopped, transaction_type());
        return;
    }

    const auto tx_fetcher = [this, transaction_hash, handle_fetch]()
    {
        const auto it = tx_find(transaction_hash);
        if (it == buffer_.end())
        {
            handle_fetch(error::not_found, transaction_type());
            return;
        }

        handle_fetch(error::success, it->tx);
    };

    strand_.queue(tx_fetcher);
}

void transaction_pool::fetch_missing_hashes(const hash_list& hashes,
    fetch_handler_missing_hashes handle_fetch)
{
    if (stopped())
    {
        handle_fetch(error::service_stopped, hash_list());
        return;
    }

    const auto tx_fetcher = [this, hashes, handle_fetch]()
    {
        hash_list missing;
        for (const auto& hash: hashes)
            if (tx_find(hash) == buffer_.end())
                missing.push_back(hash);

        handle_fetch(error::success, missing);
    };

    strand_.queue(tx_fetcher);
}

void transaction_pool::exists(const hash_digest& transaction_hash,
    exists_handler handle_exists)
{
    if (stopped())
    {
        handle_exists(error::service_stopped, false);
        return;
    }

    const auto get_existence = [this, transaction_hash, handle_exists]()
    {
        handle_exists(error::success, tx_exists(transaction_hash));
    };

    strand_.queue(get_existence);
}

// new blocks come in - remove txs in new
// old blocks taken out - resubmit txs in old
bool transaction_pool::reorganize(const std::error_code& ec,
    size_t /* fork_point */, const blockchain::block_list& new_blocks,
    const blockchain::block_list& replaced_blocks)
{
    if (ec == error::service_stopped)
    {
        log_debug(LOG_BLOCKCHAIN)
            << "Stopping transaction pool: " << ec.message();
        stop();
        return false;
    }

    if (ec)
    {
        log_debug(LOG_BLOCKCHAIN)
            << "Failure in tx pool reorganize handler: " << ec.message();
        stop();
        return false;
    }

    ////log_debug(LOG_BLOCKCHAIN)
    ////    << "Reorganize: tx pool size (" << buffer_.size()
    ////    << ") new blocks (" << new_blocks.size()
    ////    << ") replace blocks (" << replaced_blocks.size() << ")";

    if (replaced_blocks.empty())
    {
        // Remove memory pool transactions that also exist in new blocks.
        strand_.queue(
            std::bind(&transaction_pool::remove,
                this, new_blocks));
    }
    else
    {
        // See http://www.jwz.org/doc/worse-is-better.html
        // for why we take this approach. We return with an error_code.
        // An alternative would be resubmit all tx from the cleared blocks.
        strand_.queue(
            std::bind(&transaction_pool::clear,
                this, error::blockchain_reorganized));
    }

    return true;
}

// Entry methods.
// ----------------------------------------------------------------------------

// A new transaction has been received, add it to the memory pool.
void transaction_pool::add(const transaction_type& tx, confirm_handler handler)
{
    // When a new tx is added to the buffer drop the oldest.
    if (maintain_consistency_ && buffer_.size() == buffer_.capacity())
        delete_package(error::pool_filled);

    // Store a precomputed tx hash to make lookups faster.
    buffer_.push_back({ hash_transaction(tx), tx, handler });
}

// There has been a reorg, clear the memory pool.
void transaction_pool::clear(const std::error_code& ec)
{
    for (const auto& entry: buffer_)
        entry.handle_confirm(ec);

    buffer_.clear();
}

// Delete memory pool txs that are obsoleted by a new block acceptance.
void transaction_pool::remove(const blockchain::block_list& blocks)
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
    const blockchain::block_list& blocks)
{
    if (stopped() || buffer_.empty())
        return;

    for (const auto block: blocks)
        for (const auto& tx: block->transactions)
            delete_single(tx, error::success);
}

// Delete all txs that spend a previous output of any tx in the new blocks.
void transaction_pool::delete_spent_in_blocks(
    const blockchain::block_list& blocks)
{
    if (stopped() || buffer_.empty())
        return;

    for (const auto block: blocks)
        for (const auto& tx: block->transactions)
            for (const auto& input: tx.inputs)
                delete_dependencies(input.previous_output,
                    error::double_spend);
}

// Delete any tx that spends this output point.
void transaction_pool::delete_dependencies(const output_point& point,
    const std::error_code& ec)
{
    // TODO: implement output_point comparison operator.
    const auto comparison = [&point](const transaction_input_type& input)
    {
        return input.previous_output.index == point.index &&
            input.previous_output.hash == point.hash;
    };

    delete_dependencies(comparison, ec);
}

// Delete any tx that spends any output of this tx.
void transaction_pool::delete_dependencies(const hash_digest& tx_hash,
    const std::error_code& ec)
{
    const auto comparison = [&tx_hash](const transaction_input_type& input)
    {
        return input.previous_output.hash == tx_hash;
    };

    delete_dependencies(comparison, ec);
}

// This is horribly inefficient, but it's simple.
// TODO: Create persistent multi-indexed memory pool (including age and
// children) and perform this pruning trivialy (and add policy over it).
void transaction_pool::delete_dependencies(input_comparison is_dependency,
    const std::error_code& ec)
{
    std::vector<hash_digest> dependencies;
    for (const auto& entry: buffer_)
        for (const auto& input: entry.tx.inputs)
            if (is_dependency(input))
            {
                dependencies.push_back(hash_transaction(entry.tx));
                break;
            }

    // We queue deletion to protect the iterator.
    for (const auto& dependency: dependencies)
        delete_package(dependency, ec);
}

void transaction_pool::delete_package(const std::error_code& ec)
{
    if (stopped() || buffer_.empty())
        return;

    const auto& oldest = buffer_.front();
    oldest.handle_confirm(ec);
    const auto hash = oldest.hash;
    delete_package(hash, ec);
}

void transaction_pool::delete_package(const hash_digest& tx_hash,
    const code& ec)
{
    if (delete_single(tx_hash, ec))
        delete_dependencies(tx_hash, ec);
}

void transaction_pool::delete_package(const transaction_type& tx,
    const code& ec)
{
    delete_package(hash_transaction(tx), ec);
}

bool transaction_pool::delete_single(const hash_digest& tx_hash,
    const code& ec)
{
    if (stopped())
        return false;

    const auto matched = [&tx_hash](const transaction_entry_info& entry)
    {
        return entry.hash == tx_hash;
    };

    const auto it = std::find_if(buffer_.begin(), buffer_.end(), matched);
    if (it == buffer_.end())
        return false;

    it->handle_confirm(ec);
    buffer_.erase(it);
    return true;
}

bool transaction_pool::delete_single(const transaction_type& tx,
    const code& ec)
{
    return delete_single(hash_transaction(tx), ec);
}

// Deprecated, use constructor.
void transaction_pool::set_capacity(size_t capacity)
{
    //BITCOIN_ASSERT_MSG(false, 
    //    "transaction_pool::set_capacity deprecated, set on construct");
    buffer_.set_capacity(capacity);
}

} // namespace chain
} // namespace libbitcoin

