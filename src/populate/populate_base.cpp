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
#include <bitcoin/blockchain/populate/populate_base.hpp>

#include <algorithm>
#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;

#define NAME "populate_base"

// Database access is limited to:
// spend: { spender }
// transaction: { exists, height, output }

populate_base::populate_base(threadpool& pool, const fast_chain& chain)
  : dispatch_(pool, NAME),
    fast_chain_(chain)
{
}

void populate_base::populate_duplicate(size_t branch_height,
    const chain::transaction& tx) const
{
    tx.validation.duplicate = fast_chain_.get_is_unspent_transaction(
        tx.hash(), branch_height);
}

void populate_base::populate_prevout(size_t branch_height,
    const output_point& outpoint) const
{
    // The previous output will be cached on the input's outpoint.
    auto& prevout = outpoint.validation;

    prevout.spent = false;
    prevout.confirmed = false;
    prevout.cache = chain::output{};
    prevout.height = output_point::validation_type::not_specified;

    // If the input is a coinbase there is no prevout to populate.
    if (outpoint.is_null())
        return;

    size_t output_height;
    bool output_coinbase;

    // Get the script, value and spender height (if any) for the prevout.
    // The output (prevout.cache) is populated only if the return is true.
    if (!fast_chain_.get_output(prevout.cache, output_height, output_coinbase,
        outpoint, branch_height))
        return;

    //*************************************************************************
    // CONSENSUS: The genesis block coinbase may not be spent. This is the
    // consequence of satoshi not including it in the utxo set for block
    // database initialization. Only he knows why, probably an oversight.
    //*************************************************************************
    if (output_height == 0)
        return;

    // Set height only if the prevout is a coinbase tx (for maturity).
    if (output_coinbase)
        prevout.height = output_height;

    // The output is spent only if by a spend at or below the branch height.
    const auto spend_height = prevout.cache.validation.spender_height;

    // The previous output has already been spent.
    if ((spend_height <= branch_height) &&
        (spend_height != output::validation::not_spent))
    {
        prevout.spent = true;
        prevout.confirmed = true;
        prevout.cache = chain::output{};
    }
}

} // namespace blockchain
} // namespace libbitcoin
