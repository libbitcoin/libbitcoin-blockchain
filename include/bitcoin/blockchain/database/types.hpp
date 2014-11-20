/*
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
#ifndef LIBBITCOIN_BLOCKCHAIN_TYPES_HPP
#define LIBBITCOIN_BLOCKCHAIN_TYPES_HPP

#include <cstdint>

namespace libbitcoin {
    namespace chain {

typedef stealth_prefix address_bitset;

// A fixed offset location within the file.
typedef uint64_t position_type;
// An index value, usually for deriving the position.
typedef uint32_t index_type;

    } // namespace chain
} // namespace libbitcoin

#endif

