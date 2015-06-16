/**
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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

#include <cstddef>
#include <system_error>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>
#include <bitcoin/blockchain/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

transaction_pool::transaction_pool(threadpool& pool, blockchain& chain,
    size_t capacity)
  : strand_(pool), blockchain_(chain), buffer_(capacity)
{
}

transaction_pool::~transaction_pool()
{
    // This was reportedly required for use with circular_buffer.
}

bool transaction_pool::empty() const
{
    return buffer_.empty();
}

size_t transaction_pool::size() const
{
    return buffer_.size();
}

void transaction_pool::start()
{
    blockchain_.subscribe_reorganize(
        std::bind(&transaction_pool::reorganize,
            this, _1, _2, _3, _4));
}

void transaction_pool::validate(const chain::transaction& tx,
    validate_handler handle_validate)
{
    strand_.queue(&transaction_pool::do_validate,
        this, tx, handle_validate);
}
void transaction_pool::do_validate(const chain::transaction& tx,
    validate_handler handle_validate)
{
    // This must be allocated as a shared pointer reference in order for
    // start to create a shared pointer reference.
    const auto validate = std::make_shared<validate_transaction>(
        blockchain_, tx, buffer_, strand_);

    validate->start(
        strand_.wrap(&transaction_pool::validation_complete,
            this, _1, _2, tx.hash(), handle_validate));
}

void transaction_pool::validation_complete(const std::error_code& ec,
    const chain::index_list& unconfirmed, const hash_digest& tx_hash,
    validate_handler handle_validate)
{
    if (ec == error::input_not_found || ec == error::validate_inputs_failed)
    {
        BITCOIN_ASSERT(unconfirmed.size() == 1);
        //BITCOIN_ASSERT(unconfirmed[0] < tx.inputs.size());
        handle_validate(ec, unconfirmed);
    }
    else if (ec)
    {
        BITCOIN_ASSERT(unconfirmed.empty());
        handle_validate(ec, chain::index_list());
    }
    // Re-check as another transaction might have been added in the interim.
    else if (tx_exists(tx_hash))
        handle_validate(error::duplicate, chain::index_list());
    else
        handle_validate(bc::error::success, unconfirmed);
}

bool transaction_pool::tx_exists(const hash_digest& tx_hash)
{
    for (const auto& entry: buffer_)
        if (entry.hash == tx_hash)
            return true;

    return false;
}

void transaction_pool::store(const chain::transaction& tx,
    confirm_handler handle_confirm, validate_handler handle_validate)
{
    const auto store_transaction = [this, tx, handle_confirm]
    {
        // When new tx are added to the circular buffer, any tx at the front
        // will be droppped. We notify the API user through the handler.
        if (buffer_.size() == buffer_.capacity())
        {
            // There is no guarantee that handle_confirm will fire.
            const auto handle_confirm = buffer_.front().handle_confirm;
            handle_confirm(error::pool_filled);
        }

        // We store a precomputed tx hash to make lookups faster.
        buffer_.push_back({ tx.hash(), tx, handle_confirm });

        log_debug(LOG_BLOCKCHAIN)
            << "Transaction saved to mempool (" << buffer_.size() << ")";
    };

    const auto wrap_validate = [this, store_transaction, handle_validate]
        (const std::error_code& ec, const chain::index_list& unconfirmed)
    {
        if (!ec)
            store_transaction();

        handle_validate(ec, unconfirmed);
    };

    validate(tx, wrap_validate);
}

void transaction_pool::fetch(const hash_digest& transaction_hash,
    fetch_handler handle_fetch)
{
    const auto tx_fetcher = [this, transaction_hash, handle_fetch]()
    {
        for (const auto& entry: buffer_)
            if (entry.hash == transaction_hash)
            {
                handle_fetch(bc::error::success, entry.tx);
                return;
            }

        handle_fetch(error::not_found, chain::transaction());
    };

    strand_.queue(tx_fetcher);
}

void transaction_pool::exists(const hash_digest& transaction_hash,
    exists_handler handle_exists)
{
    const auto existence_tester = [this, transaction_hash, handle_exists]()
    {
        handle_exists(tx_exists(transaction_hash));
    };

    strand_.queue(existence_tester);
}

void transaction_pool::reorganize(const std::error_code& ec,
    size_t /* fork_point */, const blockchain::block_list& new_blocks,
    const blockchain::block_list& replaced_blocks)
{
    if (ec)
    {
        BITCOIN_ASSERT(ec == error::service_stopped);
        return;
    }

    log_debug(LOG_BLOCKCHAIN)
        << "Reorganize: tx pool size (" << buffer_.size()
        << ") new blocks (" << new_blocks.size()
        << ") replace blocks (" << replaced_blocks.size() << ")";

    if (replaced_blocks.empty())
        strand_.queue(
            std::bind(&transaction_pool::delete_confirmed,
                this, new_blocks));
    else
        strand_.queue(
            std::bind(&transaction_pool::invalidate_pool,
                this));

    // new blocks come in - remove txs in new
    // old blocks taken out - resubmit txs in old
    blockchain_.subscribe_reorganize(
        std::bind(&transaction_pool::reorganize,
            this, _1, _2, _3, _4));
}

void transaction_pool::invalidate_pool()
{
    // See http://www.jwz.org/doc/worse-is-better.html
    // for why we take this approach.
    // We return with an error_code and don't handle this case.
    for (const auto& entry: buffer_)
        entry.handle_confirm(error::blockchain_reorganized);

    buffer_.clear();
}

void transaction_pool::delete_confirmed(
    const blockchain::block_list& new_blocks)
{
    // Optimization: there is nothing to delete, don't loop/hash.
    if (buffer_.empty())
        return;

    for (const auto new_block: new_blocks)
        for (const auto& new_tx: new_block->transactions)
            try_delete(new_tx.hash());
}

void transaction_pool::try_delete(const hash_digest& tx_hash)
{
    for (auto it = buffer_.begin(); it != buffer_.end(); ++it)
        if (it->hash == tx_hash)
        {
            const auto handle_confirm = it->handle_confirm;
            buffer_.erase(it);
            handle_confirm(bc::error::success);
            return;
        }
}

// Deprecated, use constructor.
void transaction_pool::set_capacity(size_t capacity)
{
    //BITCOIN_ASSERT_MSG(false, 
    //    "transaction_pool::set_capacity deprecated, set on construct");
    buffer_.set_capacity(capacity);
}

} // namespace blockchain
} // namespace libbitcoin
