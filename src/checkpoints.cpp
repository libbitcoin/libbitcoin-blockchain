/**
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
#include <bitcoin/blockchain/checkpoints.hpp>

namespace libbitcoin {
    namespace chain {

inline bool checkpoint_test(
    const size_t current_height, const hash_digest& current_hash,
    const size_t checkpoint_height, const std::string& checkpoint_hex)
{
    // Not this checkpoint... Continue on with next check.
    if (current_height != checkpoint_height)
        return true;
    // Deserialize hash from hex string.
    hash_digest checkpoint_hash;
    DEBUG_ONLY(bool success =) decode_hash(checkpoint_hash, checkpoint_hex);
    BITCOIN_ASSERT_MSG(success, "Internal error: bad checkpoint hash!");
    // Both hashes should match.
    return current_hash == checkpoint_hash;
}

bool passes_checkpoints(const size_t height, const hash_digest& block_hash)
{
#define CHECKPOINT(checkpoint_height, checkpoint_hex) \
    if (!checkpoint_test(height, block_hash, \
        checkpoint_height, checkpoint_hex)) \
    { \
        return false; \
    }

#ifdef ENABLE_TESTNET
    CHECKPOINT(546,
        "000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70");
#else
    CHECKPOINT(11111,
        "0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d");
    CHECKPOINT(33333,
        "000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6");
    CHECKPOINT(68555,
        "00000000001e1b4903550a0b96e9a9405c8a95f387162e4944e8d9fbe501cd6a");
    CHECKPOINT(70567,
        "00000000006a49b14bcf27462068f1264c961f11fa2e0eddd2be0791e1d4124a");
    CHECKPOINT(74000,
        "0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20");
    CHECKPOINT(105000,
        "00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97");
    CHECKPOINT(118000,
        "000000000000774a7f8a7a12dc906ddb9e17e75d684f15e00f8767f9e8f36553");
    CHECKPOINT(134444,
        "00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe");
    CHECKPOINT(140700,
        "000000000000033b512028abb90e1626d8b346fd0ed598ac0a3c371138dce2bd");
    CHECKPOINT(168000,
        "000000000000099e61ea72015e79632f216fe6cb33d7899acb35b75c8303b763");
    CHECKPOINT(193000,
        "000000000000059f452a5f7340de6682a977387c17010ff6e6c3bd83ca8b1317");
    CHECKPOINT(210000,
        "000000000000048b95347e83192f69cf0366076336c639f9b7228e9ba171342e");
    CHECKPOINT(216116,
        "00000000000001b4f4b433e81ee46494af945cf96014816a4e2370f11b23df4e");
    CHECKPOINT(225430,
        "00000000000001c108384350f74090433e7fcf79a606b8e797f065b130575932");
    CHECKPOINT(250000,
        "000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214");
    CHECKPOINT(279000,
        "0000000000000001ae8c72a0b0c301f67e3afca10e819efa9041e458e9bd7e40");
    CHECKPOINT(295000,
        "00000000000000004d9b4ef50f0f9d686fd69db2e03af35a100370c64632a983");
    CHECKPOINT(330791,
        "000000000000000017a4b176294583519076f06cd2b5e4ef139dada8d44838d8");
    CHECKPOINT(337459,
        "000000000000000017522241d7afd686bb2315930fc1121861c9abf52e8c37f1");
#endif
    return true;
}

    } // namespace chain
} // namespace libbitcoin

