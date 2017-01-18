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
    flush_writes_(settings.flush_reorganizations),
    transaction_pool_(settings.reject_conflicts, settings.minimum_fee_satoshis),
    dispatch_(thread_pool, NAME "_dispatch"),
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
    return true;
}

bool transaction_organizer::stop()
{
    stopped_ = true;
    return true;
}

// Organize sequence.
//-----------------------------------------------------------------------------

void transaction_organizer::organize(transaction_const_ptr tx,
    result_handler handler)
{
    // TODO:
    handler(error::not_implemented);
}

void transaction_organizer::subscribe_transaction(
    transaction_handler&& handler)
{
    // TODO:
    handler(error::not_implemented, nullptr);
}

void transaction_organizer::fetch_inventory(size_t size,
    safe_chain::inventory_fetch_handler handler) const
{
    transaction_pool_.fetch_inventory(size, handler);
}

} // namespace blockchain
} // namespace libbitcoin
