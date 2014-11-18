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
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;

BOOST_AUTO_TEST_SUITE(interface_test)

void test_block_exists(const db_interface& interface,
    const size_t height, const block_type block0)
{
    const hash_digest blk_hash = hash_block_header(block0.header);
    auto r0 = interface.blocks.get(height);
    auto r0_byhash = interface.blocks.get(blk_hash);
    BOOST_REQUIRE(r0);
    BOOST_REQUIRE(r0_byhash);
    BOOST_REQUIRE(hash_block_header(r0.header()) == blk_hash);
    BOOST_REQUIRE(hash_block_header(r0_byhash.header()) == blk_hash);
    BOOST_REQUIRE(r0.height() == height);
    BOOST_REQUIRE(r0_byhash.height() == height);
    BOOST_REQUIRE(r0.transactions_size() == block0.transactions.size());
    BOOST_REQUIRE(r0_byhash.transactions_size() == block0.transactions.size());
    for (size_t i = 0; i < block0.transactions.size(); ++i)
    {
        const transaction_type& tx = block0.transactions[i];
        const hash_digest tx_hash = hash_transaction(tx);
        BOOST_REQUIRE(r0.transaction_hash(i) == tx_hash);
        BOOST_REQUIRE(r0_byhash.transaction_hash(i) == tx_hash);
        auto r0_tx = interface.transactions.get(tx_hash);
        BOOST_REQUIRE(r0_tx);
        BOOST_REQUIRE(r0_byhash);
        BOOST_REQUIRE(hash_transaction(r0_tx.transaction()) == tx_hash);
        BOOST_REQUIRE(r0_tx.height() == height);
        BOOST_REQUIRE(r0_tx.index() == i);

        if (!is_coinbase(tx))
        {
            for (size_t j = 0; j < tx.inputs.size(); ++j)
            {
                const transaction_input_type& input = tx.inputs[j];
                input_point spend{tx_hash, static_cast<uint32_t>(j)};
                auto r0_spend = interface.spends.get(input.previous_output);
                BOOST_REQUIRE(r0_spend);
                BOOST_REQUIRE(r0_spend.hash() == spend.hash);
                BOOST_REQUIRE(r0_spend.index() == spend.index);

                payment_address address;
                if (!extract(address, input.script))
                    continue;
                auto history = interface.history.get(address.hash());
                bool found = false;
                for (const auto row: history)
                {
                    if (row.point.hash == spend.hash &&
                        row.point.index == spend.index)
                    {
                        BOOST_REQUIRE(row.height == height);
                        found = true;
                        break;
                    }
                }
                BOOST_REQUIRE(found);
            }
        }
        for (size_t j = 0; j < tx.outputs.size(); ++j)
        {
            const transaction_output_type& output = tx.outputs[j];
            output_point outpoint{tx_hash, static_cast<uint32_t>(j)};

            payment_address address;
            if (!extract(address, output.script))
                continue;
            auto history = interface.history.get(address.hash());
            bool found = false;
            for (const auto row: history)
            {
                if (row.point.hash == outpoint.hash &&
                    row.point.index == outpoint.index)
                {
                    BOOST_REQUIRE(row.height == height);
                    BOOST_REQUIRE(row.value == output.value);
                    found = true;
                    break;
                }
            }
            BOOST_REQUIRE(found);
        }
    }
}

void test_block_not_exists(
    const db_interface& interface, const block_type block0)
{
    const hash_digest blk_hash = hash_block_header(block0.header);
    //auto r0_byhash = interface.blocks.get(blk_hash);
    //BOOST_REQUIRE(!r0_byhash);
    for (size_t i = 0; i < block0.transactions.size(); ++i)
    {
        const transaction_type& tx = block0.transactions[i];
        const hash_digest tx_hash = hash_transaction(tx);

        if (!is_coinbase(tx))
        {
            for (size_t j = 0; j < tx.inputs.size(); ++j)
            {
                const transaction_input_type& input = tx.inputs[j];
                input_point spend{tx_hash, static_cast<uint32_t>(j)};
                auto r0_spend = interface.spends.get(input.previous_output);
                BOOST_REQUIRE(!r0_spend);

                payment_address address;
                if (!extract(address, input.script))
                    continue;
                auto history = interface.history.get(address.hash());
                bool found = false;
                for (const auto row: history)
                {
                    if (row.point.hash == spend.hash &&
                        row.point.index == spend.index)
                    {
                        found = true;
                        break;
                    }
                }
                BOOST_REQUIRE(!found);
            }
        }
        for (size_t j = 0; j < tx.outputs.size(); ++j)
        {
            const transaction_output_type& output = tx.outputs[j];
            output_point outpoint{tx_hash, static_cast<uint32_t>(j)};

            payment_address address;
            if (!extract(address, output.script))
                continue;
            auto history = interface.history.get(address.hash());
            bool found = false;
            for (const auto row: history)
            {
                if (row.point.hash == outpoint.hash &&
                    row.point.index == outpoint.index)
                {
                    found = true;
                    break;
                }
            }
            BOOST_REQUIRE(!found);
        }
    }
}

block_type read_block(const std::string hex)
{
    data_chunk data = decode_hex(hex);
    block_type result;
    satoshi_load(data.begin(), data.end(), result);
    return result;
}

void compare_blocks(const block_type& popped, const block_type& original)
{
    BOOST_REQUIRE(hash_block_header(popped.header) ==
        hash_block_header(original.header));
    BOOST_REQUIRE(popped.transactions.size() == original.transactions.size());
    for (size_t i = 0; i < popped.transactions.size(); ++i)
    {
        BOOST_REQUIRE(hash_transaction(popped.transactions[i]) ==
            hash_transaction(original.transactions[i]));
    }
}

BOOST_AUTO_TEST_CASE(pushpop)
{
    const std::string prefix = "chain";
    boost::filesystem::create_directory(prefix);
    initialize_blockchain(prefix);

    db_paths paths(prefix);
    db_interface interface(paths, {0});
    interface.start();

    BOOST_REQUIRE(interface.blocks.last_height() ==
        block_database::null_height);

    block_type block0 = genesis_block();
    test_block_not_exists(interface, block0);
    interface.push(block0);
    test_block_exists(interface, 0, block0);

    BOOST_REQUIRE(interface.blocks.last_height() == 0);

    // Block #179
    block_type block1 = read_block(
        "01000000f2c8a8d2af43a9cd05142654e56f41d159ce0274d9cabe15a20eefb5"
        "00000000366c2a0915f05db4b450c050ce7165acd55f823fee51430a8c993e0b"
        "dbb192ede5dc6a49ffff001d192d3f2f02010000000100000000000000000000"
        "00000000000000000000000000000000000000000000ffffffff0704ffff001d"
        "0128ffffffff0100f2052a0100000043410435f0d8366085f73906a483097281"
        "55532f24293ea59fe0b33a245c4b8d75f82c3e70804457b7f49322aa822196a7"
        "521e4931f809d7e489bccb4ff14758d170e5ac000000000100000001169e1e83"
        "e930853391bc6f35f605c6754cfead57cf8387639d3b4096c54f18f401000000"
        "48473044022027542a94d6646c51240f23a76d33088d3dd8815b25e9ea18cac6"
        "7d1171a3212e02203baf203c6e7b80ebd3e588628466ea28be572fe1aaa3f309"
        "47da4763dd3b3d2b01ffffffff0200ca9a3b00000000434104b5abd412d4341b"
        "45056d3e376cd446eca43fa871b51961330deebd84423e740daa520690e1d9e0"
        "74654c59ff87b408db903649623e86f1ca5412786f61ade2bfac005ed0b20000"
        "000043410411db93e1dcdb8a016b49840f8c53bc1eb68a382e97b1482ecad7b1"
        "48a6909a5cb2e0eaddfb84ccf9744464f82e160bfa9b8b64f9d4c03f999b8643"
        "f656b412a3ac00000000");
    test_block_not_exists(interface, block1);
    interface.push(block1);
    test_block_exists(interface, 1, block1);

    BOOST_REQUIRE(interface.blocks.last_height() == 1);

    // Block #181
    block_type block2 = read_block(
        "01000000e5c6af65c46bd826723a83c1c29d9efa189320458dc5298a0c8655dc"
        "0000000030c2a0d34bfb4a10d35e8166e0f5a37bce02fc1b85ff983739a19119"
        "7f010f2f40df6a49ffff001d2ce7ac9e02010000000100000000000000000000"
        "00000000000000000000000000000000000000000000ffffffff0704ffff001d"
        "0129ffffffff0100f2052a01000000434104b10dd882c04204481116bd4b4151"
        "0e98c05a869af51376807341fc7e3892c9034835954782295784bfc763d9736e"
        "d4122c8bb13d6e02c0882cb7502ce1ae8287ac000000000100000001be141eb4"
        "42fbc446218b708f40caeb7507affe8acff58ed992eb5ddde43c6fa101000000"
        "4847304402201f27e51caeb9a0988a1e50799ff0af94a3902403c3ad4068b063"
        "e7b4d1b0a76702206713f69bd344058b0dee55a9798759092d0916dbbc3e592f"
        "ee43060005ddc17401ffffffff0200e1f5050000000043410401518fa1d1e1e3"
        "e162852d68d9be1c0abad5e3d6297ec95f1f91b909dc1afe616d6876f9291845"
        "1ca387c4387609ae1a895007096195a824baf9c38ea98c09c3ac007ddaac0000"
        "000043410411db93e1dcdb8a016b49840f8c53bc1eb68a382e97b1482ecad7b1"
        "48a6909a5cb2e0eaddfb84ccf9744464f82e160bfa9b8b64f9d4c03f999b8643"
        "f656b412a3ac00000000");
    test_block_not_exists(interface, block2);
    interface.push(block2);
    test_block_exists(interface, 2, block2);

    BOOST_REQUIRE(interface.blocks.last_height() == 2);

    // Block #183
    block_type block3 = read_block(
        "01000000bed482ccb42bf5c20d00a5bb9f7d688e97b94c622a7f42f3aaf23f8b"
        "000000001cafcb3e4cad2b4eed7fb7fcb7e49887d740d66082eb45981194c532"
        "b58d475258ee6a49ffff001d1bc0e23202010000000100000000000000000000"
        "00000000000000000000000000000000000000000000ffffffff0704ffff001d"
        "011affffffff0100f2052a0100000043410435d66d6cef63a3461110c810975b"
        "8816308372b58274d88436a974b478d98d8d972f7233ea8a5242d151de9d4b1a"
        "c11a6f7f8460e8f9b146d97c7bad980cc5ceac000000000100000001ba91c1d5"
        "e55a9e2fab4e41f55b862a73b24719aad13a527d169c1fad3b63b51200000000"
        "48473044022041d56d649e3ca8a06ffc10dbc6ba37cb958d1177cc8a155e83d0"
        "646cd5852634022047fd6a02e26b00de9f60fb61326856e66d7a0d5e2bc9d01f"
        "b95f689fc705c04b01ffffffff0100e1f50500000000434104fe1b9ccf732e1f"
        "6b760c5ed3152388eeeadd4a073e621f741eb157e6a62e3547c8e939abbd6a51"
        "3bf3a1fbe28f9ea85a4e64c526702435d726f7ff14da40bae4ac00000000");
    test_block_not_exists(interface, block3);
    interface.push(block3);
    test_block_exists(interface, 3, block3);

    BOOST_REQUIRE(interface.blocks.last_height() == 3);

    block_type block3_popped = interface.pop();
    BOOST_REQUIRE(interface.blocks.last_height() == 2);
    compare_blocks(block3_popped, block3);

    test_block_not_exists(interface, block3);
    test_block_exists(interface, 2, block2);
    test_block_exists(interface, 1, block1);
    test_block_exists(interface, 0, block0);

    block_type block2_popped = interface.pop();
    BOOST_REQUIRE(interface.blocks.last_height() == 1);
    compare_blocks(block2_popped, block2);

    test_block_not_exists(interface, block3);
    test_block_not_exists(interface, block2);
    test_block_exists(interface, 1, block1);
    test_block_exists(interface, 0, block0);
}

BOOST_AUTO_TEST_SUITE_END()

