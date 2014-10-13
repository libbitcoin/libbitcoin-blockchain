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
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;

BOOST_AUTO_TEST_SUITE(databases_test)

BOOST_AUTO_TEST_CASE(spend_db_test)
{
    output_point key1{
        decode_hash("4129e76f363f9742bc98dd3d40c99c90"
                    "66e4d53b8e10e5097bd6f7b5059d7c53"), 110};
    output_point key2{
        decode_hash("eefa5d23968584be9d8d064bcf99c246"
                    "66e4d53b8e10e5097bd6f7b5059d7c53"), 4};
    output_point key3{
        decode_hash("4129e76f363f9742bc98dd3d40c99c90"
                    "eefa5d23968584be9d8d064bcf99c246"), 8};
    output_point key4{
        decode_hash("80d9e7012b5b171bf78e75b52d2d1495"
                    "80d9e7012b5b171bf78e75b52d2d1495"), 9};

    input_point val1{
        decode_hash("4742b3eac32d35961f9da9d42d495ff1"
                    "d90aba96944cac3e715047256f7016d1"), 0};
    input_point val2{
        decode_hash("d90aba96944cac3e715047256f7016d1"
                    "d90aba96944cac3e715047256f7016d1"), 0};
    input_point val3{
        decode_hash("3cc768bbaef30587c72c6eba8dbf6aee"
                    "c4ef24172ae6fe357f2e24c2b0fa44d5"), 0};
    input_point val4{
        decode_hash("4742b3eac32d35961f9da9d42d495ff1"
                    "3cc768bbaef30587c72c6eba8dbf6aee"), 0};

    touch_file("spend_db");
    spend_database db("spend_db");
    db.initialize_new();
    db.start();
    db.store(key1, val1);
    db.store(key2, val2);
    db.store(key3, val3);
    // Test fetch.
    auto res1 = db.get(key1);
    BOOST_REQUIRE(res1);
    BOOST_REQUIRE(res1.hash() == val1.hash && res1.index() == val1.index);
    auto res2 = db.get(key2);
    BOOST_REQUIRE(res2);
    BOOST_REQUIRE(res2.hash() == val2.hash && res2.index() == val2.index);
    auto res3 = db.get(key3);
    BOOST_REQUIRE(res3);
    BOOST_REQUIRE(res3.hash() == val3.hash && res3.index() == val3.index);
    // Record shouldnt exist yet.
    BOOST_REQUIRE(!db.get(key4));
    // Delete record.
    db.remove(key3);
    BOOST_REQUIRE(!db.get(key3));
    // Add another record.
    db.store(key4, val4);
    // Fetch it.
    auto res4 = db.get(key4);
    BOOST_REQUIRE(res4);
    BOOST_REQUIRE(res4.hash() == val4.hash && res4.index() == val4.index);
    db.sync();
}

transaction_type random_tx(size_t fudge)
{
    block_type genesis = genesis_block();
    transaction_type result = genesis.transactions[0];
    result.inputs[0].previous_output.index = fudge;
    return result;
}

BOOST_AUTO_TEST_CASE(block_db_test)
{
    block_type block0 = genesis_block();
    block0.transactions.push_back(random_tx(0));
    block0.transactions.push_back(random_tx(1));
    const hash_digest h0 = hash_block_header(block0.header);

    block_type block1;
    block1.header = block0.header;
    block1.header.nonce = 4;
    block1.transactions.push_back(random_tx(2));
    block1.transactions.push_back(random_tx(3));
    block1.transactions.push_back(random_tx(4));
    block1.transactions.push_back(random_tx(5));
    const hash_digest h1 = hash_block_header(block1.header);

    block_type block2;
    block2.header = block0.header;
    block2.header.nonce = 110;
    block2.transactions.push_back(random_tx(6));
    block2.transactions.push_back(random_tx(7));
    block2.transactions.push_back(random_tx(8));
    block2.transactions.push_back(random_tx(9));
    block2.transactions.push_back(random_tx(10));
    const hash_digest h2 = hash_block_header(block2.header);

    block_type block3;
    block3.header = block0.header;
    block3.header.nonce = 88;
    block3.transactions.push_back(random_tx(11));
    block3.transactions.push_back(random_tx(12));
    block3.transactions.push_back(random_tx(13));
    const hash_digest h3 = hash_block_header(block3.header);

    block_type block4a;
    block4a.header = block0.header;
    block4a.header.nonce = 63;
    block4a.transactions.push_back(random_tx(14));
    block4a.transactions.push_back(random_tx(15));
    block4a.transactions.push_back(random_tx(16));
    const hash_digest h4a = hash_block_header(block4a.header);

    block_type block5a;
    block5a.header = block0.header;
    block5a.header.nonce = 99;
    block5a.transactions.push_back(random_tx(17));
    block5a.transactions.push_back(random_tx(18));
    block5a.transactions.push_back(random_tx(19));
    block5a.transactions.push_back(random_tx(20));
    block5a.transactions.push_back(random_tx(21));
    const hash_digest h5a = hash_block_header(block5a.header);

    block_type block4b;
    block4b.header = block0.header;
    block4b.header.nonce = 633;
    block4b.transactions.push_back(random_tx(22));
    block4b.transactions.push_back(random_tx(23));
    block4b.transactions.push_back(random_tx(24));
    const hash_digest h4b = hash_block_header(block4b.header);

    block_type block5b;
    block5b.header = block0.header;
    block5b.header.nonce = 222;
    block5b.transactions.push_back(random_tx(25));
    block5b.transactions.push_back(random_tx(26));
    block5b.transactions.push_back(random_tx(27));
    block5b.transactions.push_back(random_tx(28));
    block5b.transactions.push_back(random_tx(29));
    const hash_digest h5b = hash_block_header(block5b.header);

    touch_file("block_db_lookup");
    touch_file("block_db_rows");
    block_database db("block_db_lookup", "block_db_rows");
    db.initialize_new();
    db.start();
    BOOST_REQUIRE(db.last_height() == block_database::null_height);
    db.store(block0);
    db.store(block1);
    db.store(block2);
    db.store(block3);
    BOOST_REQUIRE(db.last_height() == 3);
    // Fetch block 2 by hash.
    auto res_h2 = db.get(h2);
    BOOST_REQUIRE(res_h2);
    BOOST_REQUIRE(hash_block_header(res_h2.header()) == h2);
    for (size_t i = 0; i < res_h2.transactions_size(); ++i)
        BOOST_REQUIRE(res_h2.transaction_hash(i) ==
            hash_transaction(block2.transactions[i]));
    // Try a fork event.
    db.store(block4a);
    db.store(block5a);
    // Fetch blocks.
    auto res4a = db.get(4);
    BOOST_REQUIRE(res4a);
    BOOST_REQUIRE(hash_block_header(res4a.header()) == h4a);
    auto res5a = db.get(5);
    BOOST_REQUIRE(res5a);
    BOOST_REQUIRE(hash_block_header(res5a.header()) == h5a);
    // Unlink old chain.
    BOOST_REQUIRE(db.last_height() == 5);
    db.unlink(4);
    BOOST_REQUIRE(db.last_height() == 3);
    // Block 3 exists.
    auto res3_exists = db.get(3);
    BOOST_REQUIRE(res3_exists);
    // No blocks exist now.
    auto res4_none = db.get(4);
    BOOST_REQUIRE(!res4_none);
    auto res5_none = db.get(5);
    BOOST_REQUIRE(!res5_none);
    // Add new blocks.
    db.store(block4b);
    db.store(block5b);
    BOOST_REQUIRE(db.last_height() == 5);
    // Fetch blocks.
    auto res4b = db.get(4);
    BOOST_REQUIRE(res4b);
    BOOST_REQUIRE(hash_block_header(res4b.header()) == h4b);
    auto res5b = db.get(5);
    BOOST_REQUIRE(res5b);
    BOOST_REQUIRE(hash_block_header(res5b.header()) == h5b);
    for (size_t i = 0; i < res5b.transactions_size(); ++i)
        BOOST_REQUIRE(res5b.transaction_hash(i) ==
            hash_transaction(block5b.transactions[i]));
    // Test also fetch by hash.
    auto res_h5b = db.get(h5b);
    BOOST_REQUIRE(res_h5b);
    BOOST_REQUIRE(hash_block_header(res_h5b.header()) == h5b);
    db.sync();
}

BOOST_AUTO_TEST_CASE(transaction_db_test)
{
    transaction_metainfo info1{110, 88};
    transaction_type tx1;
    data_chunk raw_tx1 = decode_hex(
        "0100000001537c9d05b5f7d67b09e5108e3bd5e466909cc9403ddd98bc42973f"
        "366fe729410600000000ffffffff0163000000000000001976a914fe06e7b4c8"
        "8a719e92373de489c08244aee4520b88ac00000000");
    satoshi_load(raw_tx1.begin(), raw_tx1.end(), tx1);
    const hash_digest h1 = hash_transaction(tx1);

    transaction_metainfo info2{4, 6};
    transaction_type tx2;
    data_chunk raw_tx2 = decode_hex(
        "010000000147811c3fc0c0e750af5d0ea7343b16ea2d0c291c002e3db7786692"
        "16eb689de80000000000ffffffff0118ddf505000000001976a914575c2f0ea8"
        "8fcbad2389a372d942dea95addc25b88ac00000000");
    satoshi_load(raw_tx2.begin(), raw_tx2.end(), tx2);
    const hash_digest h2 = hash_transaction(tx2);

    touch_file("tx_db_map");
    transaction_database db("tx_db_map");
    db.initialize_new();
    db.start();
    db.store(info1, tx1);
    db.store(info2, tx2);
    auto res1 = db.get(h1);
    BOOST_REQUIRE(hash_transaction(res1.transaction()) == h1);
    auto res2 = db.get(h2);
    BOOST_REQUIRE(hash_transaction(res2.transaction()) == h2);
    db.sync();
}

BOOST_AUTO_TEST_CASE(history_db_test)
{
    const short_hash key1 = decode_short_hash(
        "a006500b7ddfd568e2b036c65a4f4d6aaa0cbd9b");
    output_point out11{
        decode_hash("4129e76f363f9742bc98dd3d40c99c90"
                    "66e4d53b8e10e5097bd6f7b5059d7c53"), 110};
    const uint32_t out_h11 = 110;
    const uint64_t val11 = 4;
    output_point out12{
        decode_hash("eefa5d23968584be9d8d064bcf99c246"
                    "66e4d53b8e10e5097bd6f7b5059d7c53"), 4};
    const uint32_t out_h12 = 120;
    const uint64_t val12 = 8;
    output_point out13{
        decode_hash("4129e76f363f9742bc98dd3d40c99c90"
                    "eefa5d23968584be9d8d064bcf99c246"), 8};
    const uint32_t out_h13 = 222;
    const uint64_t val13 = 6;

    input_point spend11{
        decode_hash("4742b3eac32d35961f9da9d42d495ff1"
                    "d90aba96944cac3e715047256f7016d1"), 0};
    const uint32_t spend_h11 = 115;
    input_point spend13{
        decode_hash("3cc768bbaef30587c72c6eba8dbf6aee"
                    "c4ef24172ae6fe357f2e24c2b0fa44d5"), 0};
    const uint32_t spend_h13 = 320;

    const short_hash key2 = decode_short_hash(
        "9c6b3bdaa612ceab88d49d4431ed58f26e69b90d");
    output_point out21{
        decode_hash("80d9e7012b5b171bf78e75b52d2d1495"
                    "80d9e7012b5b171bf78e75b52d2d1495"), 9};
    const uint32_t out_h21 = 3982;
    const uint64_t val21 = 65;
    output_point out22{
        decode_hash("4742b3eac32d35961f9da9d42d495ff1"
                    "3cc768bbaef30587c72c6eba8dbf6aee"), 0};
    const uint32_t out_h22 = 78;
    const uint64_t val22 = 9;

    input_point spend22{
        decode_hash("3cc768bbaef30587c72c6eba8dbfffff"
                    "c4ef24172ae6fe357f2e24c2b0fa44d5"), 0};
    const uint32_t spend_h22 = 900;

    const short_hash key3 = decode_short_hash(
        "3eb84f6a98478e516325b70fecf9903e1ce7528b");
    output_point out31{
        decode_hash("d90aba96944cac3e715047256f7016d1"
                    "d90aba96944cac3e715047256f7016d1"), 0};
    const uint32_t out_h31 = 378;
    const uint64_t val31 = 34;

    const short_hash key4 = decode_short_hash(
        "d60db39ca8ce4caf0f7d2b7d3111535d9543473f");
    output_point out42{
        decode_hash("aaaaaaaaaaacac3e715047256f7016d1"
                    "d90aaa96944cac3e715047256f7016d1"), 0};
    const uint32_t out_h41 = 74448;
    const uint64_t val41 = 990;

    touch_file("history_db_lookup");
    touch_file("history_db_rows");
    history_database db("history_db_lookup", "history_db_rows");
    db.initialize_new();
    db.start();
    db.add_row(key1, out11, out_h11, val11);
    db.add_row(key1, out12, out_h12, val12);
    db.add_row(key1, out13, out_h13, val13);
    db.add_spend(key1, out11, spend11, spend_h11);
    db.add_spend(key1, out13, spend13, spend_h13);

    db.add_row(key2, out21, out_h21, val21);
    db.add_row(key2, out22, out_h22, val22);

    auto fetch_s1 = [=](const blockchain::history_list& history)
    {
        BOOST_REQUIRE(history.size() == 3);
        BOOST_REQUIRE(history[2].output.hash == out11.hash);
        BOOST_REQUIRE(history[2].output.index == out11.index);
        BOOST_REQUIRE(history[2].output_height == out_h11);
        BOOST_REQUIRE(history[2].value == val11);
        BOOST_REQUIRE(history[2].spend.hash == spend11.hash);
        BOOST_REQUIRE(history[2].spend.index == spend11.index);
        BOOST_REQUIRE(history[2].spend_height == spend_h11);

        BOOST_REQUIRE(history[1].output.hash == out12.hash);
        BOOST_REQUIRE(history[1].output.index == out12.index);
        BOOST_REQUIRE(history[1].output_height == out_h12);
        BOOST_REQUIRE(history[1].value == val12);
        BOOST_REQUIRE(history[1].spend_height == 0);

        BOOST_REQUIRE(history[0].output.hash == out13.hash);
        BOOST_REQUIRE(history[0].output.index == out13.index);
        BOOST_REQUIRE(history[0].output_height == out_h13);
        BOOST_REQUIRE(history[0].value == val13);
        BOOST_REQUIRE(history[0].spend.hash == spend13.hash);
        BOOST_REQUIRE(history[0].spend.index == spend13.index);
        BOOST_REQUIRE(history[0].spend_height == spend_h13);
    };
    auto res_s1 = db.get(key1);
    fetch_s1(res_s1.history);
    auto no_spend = [=](const blockchain::history_list& history)
    {
        BOOST_REQUIRE(history[0].spend_height == 0);
        BOOST_REQUIRE(history[1].spend_height == 0);
    };
    auto res_ns = db.get(key2);
    no_spend(res_ns.history);
    db.add_spend(key2, out22, spend22, spend_h22);
    auto has_spend = [=](const blockchain::history_list& history)
    {
        BOOST_REQUIRE(history[0].output.hash == out22.hash);
        BOOST_REQUIRE(history[0].output.index == out22.index);
        BOOST_REQUIRE(history[0].output_height == out_h22);
        BOOST_REQUIRE(history[0].value == val22);
        BOOST_REQUIRE(history[0].spend.hash == spend22.hash);
        BOOST_REQUIRE(history[0].spend.index == spend22.index);
        BOOST_REQUIRE(history[0].spend_height == spend_h22);

        BOOST_REQUIRE(history[1].spend_height == 0);
    };
    auto res_has_sp = db.get(key2);
    has_spend(res_has_sp.history);
    db.delete_spend(key2, spend22);
    auto res_no_sp = db.get(key2);
    no_spend(res_no_sp.history);

    db.add_row(key3, out31, out_h31, val31);
    db.add_row(key4, out31, out_h41, val41);
    auto has_one_row = [=](const blockchain::history_list& history)
    {
        BOOST_REQUIRE(history.size() == 1);
    };
    auto res_1r1 = db.get(key3);
    has_one_row(res_1r1.history);
    auto res_1r2 = db.get(key4);
    has_one_row(res_1r2.history);
    auto has_no_rows = [=](const blockchain::history_list& history)
    {
        BOOST_REQUIRE(history.empty());
    };
    db.delete_last_row(key3);
    auto res_1nr1 = db.get(key3);
    has_no_rows(res_1nr1.history);
    auto res_1nr2 = db.get(key4);
    has_one_row(res_1nr2.history);

    db.sync();
}

BOOST_AUTO_TEST_SUITE_END()

