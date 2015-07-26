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
#include <bitcoin/blockchain/blockchain.hpp>

#include <cstdint>
#include <system_error>

namespace libbitcoin {
namespace chain {

// Fast modulus calculation where divisor is a power of 2.
static uint64_t remainder_fast(const hash_digest& value,
    const uint64_t divisor)
{
    BITCOIN_ASSERT(divisor % 2 == 0);

    // Only use the first 8 bytes of hash value for this calculation.
    const auto hash_value = from_little_endian_unsafe<uint64_t>(value.begin());

    // x mod 2**n == x & (2**n - 1)
    return hash_value & (divisor - 1);
}

uint64_t spend_checksum(output_point outpoint)
{
    // Assuming outpoint hash is sufficiently random, this method works well
    // for generating row checksums. Max pow2 value for a uint64_t is 1 << 63.
    constexpr uint64_t divisor = uint64_t{1} << 63;
    static_assert(divisor == 9223372036854775808ull, "Wrong divisor value.");

    // Write index onto outpoint hash.
    auto serial = make_serializer(outpoint.hash.begin());
    serial.write_4_bytes(outpoint.index);

    // Collapse it into uint64_t.
    return remainder_fast(outpoint.hash, divisor);
}

} // namespace chain
} // namespace libbitcoin

