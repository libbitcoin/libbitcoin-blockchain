/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/pools/block_pool.hpp>

#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace blockchain {

block_pool::block_pool()
{
    // Make thread safe for add/remove (distinct add/remove mutexes).
    // Store pointer by height (sorted/unique) and hash (unique).
    //
    // Add blocks under blockchain populated critical section.
    // Remove under blockchain candidated critical section.
    //
    // Limit to configured entry count (size) when adding.
    // Clear heights at/below new add height to mitigate turds.
    //
    // Trigger read-ahead population when reading and below configured count
    // while the max height is below the current populated top candidate and
    // not currently reading ahead. Fan out read-ahead modulo network cores.
}

} // namespace blockchain
} // namespace libbitcoin
