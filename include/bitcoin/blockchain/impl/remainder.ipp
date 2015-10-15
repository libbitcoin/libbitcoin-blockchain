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
#ifndef LIBBITCOIN_BLOCKCHAIN_UTILITY_IPP
#define LIBBITCOIN_BLOCKCHAIN_UTILITY_IPP

#include <cstdint>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace blockchain {

template <typename HashType>
uint32_t remainder(const HashType& value, const uint32_t divisor)
{
    static_assert(sizeof(HashType) >= sizeof(uint32_t), "HashType too small.");
    if (divisor == 0)
        return 0;

    const auto hash32 = from_big_endian_unsafe<uint32_t>(value.begin());
    return hash32 % divisor;
}

template <typename HashType>
uint64_t remainder(const HashType& value, const uint64_t divisor)
{
    static_assert(sizeof(HashType) >= sizeof(uint64_t), "HashType too small.");
    if (divisor == 0)
        return 0;

    const auto hash64 = from_big_endian_unsafe<uint64_t>(value.begin());
    return hash64 % divisor;
}

} // namespace blockchain
} // namespace libbitcoin

#endif
