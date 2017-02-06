/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/pools/transaction_organizer.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace std::placeholders;

#define NAME "transaction_organizer"

transaction_organizer::transaction_organizer(threadpool& thread_pool,
    fast_chain& chain, const settings& settings)
  : fast_chain_(chain),
    stopped_(true),
    transaction_pool_(settings),
    validator_(thread_pool, fast_chain_, settings),
    subscriber_(std::make_shared<transaction_subscriber>(thread_pool, NAME))
{
}

// Properties.
//-----------------------------------------------------------------------------

bool transaction_organizer::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

bool transaction_organizer::start()
{
    stopped_ = false;
    subscriber_->start();
    validator_.start();
    return true;
}

bool transaction_organizer::stop()
{
    validator_.stop();
    subscriber_->stop();
    subscriber_->invoke(error::service_stopped, {});
    stopped_ = true;
    return true;
}

bool transaction_organizer::close()
{
    BITCOIN_ASSERT(stopped_);
    return true;
}

// Organize sequence.
//-----------------------------------------------------------------------------

// This is called from block_chain::organize.
void transaction_organizer::organize(transaction_const_ptr tx,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Checks that are independent of chain state.
    const auto ec = validator_.check(tx);

    if (ec)
    {
        handler(ec);
        return;
    }

    const auto accept_handler =
        std::bind(&transaction_organizer::handle_accept,
            this, _1, tx, handler);

    // Checks that are dependent on chain state and prevouts.
    validator_.accept(tx, accept_handler);
}

// Verify sub-sequence.
//-----------------------------------------------------------------------------

// private
void transaction_organizer::handle_accept(const code& ec,
    transaction_const_ptr tx, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        handler(ec);
        return;
    }

    const result_handler connect_handler =
        std::bind(&transaction_organizer::handle_connect,
            this, _1, tx, handler);

    // Checks that include script validation.
    validator_.connect(tx, connect_handler);
}

// private
void transaction_organizer::handle_connect(const code& ec,
    transaction_const_ptr tx, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        handler(ec);
        return;
    }

    // The validation is not intended to store the transaction.
    // TODO: create an simulated validation path that does not lock others.
    if (tx->validation.simulate)
    {
        handler(error::success);
        return;
    }

    const auto complete =
        std::bind(&transaction_organizer::handle_transaction,
            this, _1, tx, handler);

    //#########################################################################
    fast_chain_.push(tx, complete);
    //#########################################################################
}

// private
void transaction_organizer::handle_transaction(const code& ec,
    transaction_const_ptr tx, result_handler handler)
{
    if (ec == error::unspent_duplicate)
    {
        handler(ec);
        return;
    }

    if (ec)
    {
        LOG_FATAL(LOG_BLOCKCHAIN)
            << "Failure writing transaction to store, is now corrupted: "
            << ec.message();
        handler(ec);
        return;
    }

    // This gets picked up by node tx-out protocol for announcement to peers.
    notify_transaction(tx);

    // This is the end of the tx verify sub-sequence.
    handler(ec);
}

// Subscription.
//-----------------------------------------------------------------------------

void transaction_organizer::subscribe_transaction(
    transaction_handler&& handler)
{
    subscriber_->subscribe(std::move(handler), error::service_stopped, {});
}

// private
void transaction_organizer::notify_transaction(transaction_const_ptr tx)
{
    // Invoke is required here to prevent subscription parsing from creating a
    // unsurmountable backlog during mempool message handling, etc.
    subscriber_->invoke(error::success, tx);
}

// Queries.
//-----------------------------------------------------------------------------

void transaction_organizer::fetch_template(
    merkle_block_fetch_handler handler) const
{
    transaction_pool_.fetch_template(handler);
}

void transaction_organizer::fetch_mempool(size_t maximum,
    inventory_fetch_handler handler) const
{
    transaction_pool_.fetch_mempool(maximum, handler);
}

// Utility.
//-----------------------------------------------------------------------------

} // namespace blockchain
} // namespace libbitcoin
