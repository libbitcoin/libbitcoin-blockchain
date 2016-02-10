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
#include <bitcoin/blockchain/block.hpp>

#include <cstdint>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace blockchain {

static const std::string encoded_mainnet_genesis_block =
    "01000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"
    "29ab5f49"
    "ffff001d"
    "1dac2b7c"
    "01"
    "01000000"
    "01"
    "0000000000000000000000000000000000000000000000000000000000000000ffffffff"
    "4d"
    "04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73"
    "ffffffff"
    "01"
    "00f2052a01000000"
    "43"
    "4104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac"
    "00000000";

static const std::string encoded_testnet_genesis_block =
    "01000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"
    "dae5494d"
    "ffff001d"
    "1aa4ae18"
    "01"
    "01000000"
    "01"
    "0000000000000000000000000000000000000000000000000000000000000000ffffffff"
    "4d"
    "04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73"
    "ffffffff"
    "01"
    "00f2052a01000000"
    "43"
    "4104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac"
    "00000000";

chain::block mainnet_genesis_block()
{
    data_chunk raw_block;
    decode_base16(raw_block, encoded_mainnet_genesis_block);
    const auto genesis = chain::block::factory_from_data(raw_block);

    BITCOIN_ASSERT(genesis.is_valid());
    BITCOIN_ASSERT(genesis.transactions.size() == 1);
    BITCOIN_ASSERT(chain::block::generate_merkle_root(genesis.transactions)
        == genesis.header.merkle);

    return genesis;
}

chain::block testnet_genesis_block()
{
    data_chunk raw_block;
    decode_base16(raw_block, encoded_testnet_genesis_block);
    const auto genesis = chain::block::factory_from_data(raw_block);

    BITCOIN_ASSERT(genesis.is_valid());
    BITCOIN_ASSERT(genesis.transactions.size() == 1);
    BITCOIN_ASSERT(chain::block::generate_merkle_root(genesis.transactions)
        == genesis.header.merkle);

    return genesis;
}

index_list block_locator_indexes(size_t top_height)
{
    BITCOIN_ASSERT(top_height <= bc::max_int64);
    const auto top_height64 = static_cast<int64_t>(top_height);

    index_list indexes;

    // Modify the step in the iteration.
    int64_t step = 1;

    // Start at the top of the chain and work backwards.
    for (auto index = top_height64; index > 0; index -= step)
    {
        // Push top 10 indexes first, then back off exponentially.
        if (indexes.size() >= 10)
            step <<= 1;

        indexes.push_back(static_cast<size_t>(index));
    }

    //  Push the genesis block index.
    indexes.push_back(0);
    return indexes;
}

uint64_t block_mint(size_t height)
{
    uint64_t subsidy = coin_price(initial_block_reward);
    subsidy >>= (height / reward_interval);
    return subsidy;
}

hash_number block_work(uint32_t bits)
{
    hash_number target;

    if (!target.set_compact(bits))
        return 0;

    if (target == 0)
        return 0;

    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~target / (target + 1)) + 1;
}

// Fast modulus calculation where divisor is a power of 2.
static uint64_t remainder(const hash_digest& value, const uint64_t divisor)
{
    BITCOIN_ASSERT(divisor % 2 == 0);

    // Only use the first 8 bytes of hash value for this calculation.
    const auto hash_value = from_little_endian_unsafe<uint64_t>(value.begin());

    // x mod 2**n == x & (2**n - 1)
    return hash_value & (divisor - 1);
}

uint64_t checksum(chain::output_point outpoint)
{
    // Assuming outpoint hash is sufficiently random, this method works well
    // for generating row checksums. Max pow2 value for a uint64_t is 1 << 63.
    static constexpr uint64_t divisor = uint64_t{ 1 } << 63;
    static_assert(divisor == 9223372036854775808ull, "Wrong divisor value.");

    // Write index onto outpoint hash.
    auto serial = make_serializer(outpoint.hash.begin());
    serial.write_4_bytes_little_endian(outpoint.index);

    // Collapse it into uint64_t.
    return remainder(outpoint.hash, divisor);
}

} // namespace blockchain
} // namespace libbitcoin
