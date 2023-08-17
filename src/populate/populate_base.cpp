/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/populate/populate_base.hpp>

#include <algorithm>
#include <cstddef>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::database;

#define NAME "populate_base"

// Database access is limited to:
// spend: { spender }
// transaction: { exists, height, position, output }

populate_base::populate_base(dispatcher& dispatch, const fast_chain& chain)
  : dispatch_(dispatch),
    fast_chain_(chain)
{
}

// This is the only necessary file system read in block/tx validation.
void populate_base::populate_duplicate(size_t branch_height,
    const chain::transaction& tx, bool require_confirmed) const
{
    tx.validation.duplicate = fast_chain_.get_is_unspent_transaction(
        tx.hash(), branch_height, require_confirmed);
}

void populate_base::populate_pooled(const chain::transaction& tx,
    uint32_t forks) const
{
    size_t height;
    size_t position;

    if (fast_chain_.get_transaction_position(height, position, tx.hash(),
        false) && (position == transaction_database::unconfirmed))
    {
        tx.validation.pooled = true;
        tx.validation.current = (height == forks);
        return;
    }

    tx.validation.pooled = false;
    tx.validation.current = false;
}

// Unspent outputs are cached by the store. If the cache is large enough this
// may never hit the file system. However on high RAM systems the file system
// is faster than the cache due to reduced paging of the memory-mapped file.
void populate_base::populate_prevout(size_t branch_height,
    const output_point& outpoint, bool require_confirmed) const
{
    // The previous output will be cached on the input's outpoint.
    auto& prevout = outpoint.validation;

    prevout.spent = false;
    prevout.confirmed = false;
    prevout.cache = chain::output{};

    // If the input is a coinbase there is no prevout to populate.
    if (outpoint.is_null())
        return;

    // Get the prevout/cache (and spender height) and its metadata.
    // The output (prevout.cache) is populated only if the return is true.
    if (!fast_chain_.get_output(prevout.cache, prevout.height,
        prevout.median_time_past, prevout.coinbase, outpoint, branch_height,
        require_confirmed))
        return;

    //*************************************************************************
    // CONSENSUS: The genesis block coinbase may not be spent. This is the
    // consequence of satoshi not including it in the utxo set for block
    // database initialization. Only he knows why, probably an oversight.
    //*************************************************************************
    if (prevout.height == 0)
        return;

    // BUGBUG: Spends are not marked as spent by unconfirmed transactions.
    // So tx pool transactions currently have no double spend limitation.
    // The output is spent only if by a spend at or below the branch height.
    const auto spend_height = prevout.cache.validation.spender_height;

    // The previous output has already been spent (double spend).
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
