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
#ifndef LIBBITCOIN_BLOCKCHAIN_UTILITY_IPP
#define LIBBITCOIN_BLOCKCHAIN_UTILITY_IPP

#include <gmp.h>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
    namespace chain {

template <typename HashType>
uint64_t remainder(const HashType& value, const uint64_t divisor)
{
    mpz_t integ;
    mpz_init(integ);
    mpz_import(integ, value.size(), 1, 1, 1, 0, value.data());
    uint64_t remainder = mpz_fdiv_ui(integ, divisor);
    mpz_clear(integ);
    return remainder;
}

template <typename HashType>
uint64_t remainder_fast(const HashType& value, const uint64_t divisor)
{
    BITCOIN_ASSERT(divisor % 2 == 0);
    // Only use the first 8 bytes of hash value for this calculation.
    uint64_t hash_value = from_little_endian<uint64_t>(value.begin());
    // x mod 2**n == x & (2**n - 1)
    return hash_value & (divisor - 1);
}

    } // namespace chain
} // namespace libbitcoin

#endif

