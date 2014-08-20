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
#include <bitcoin/bitcoin.hpp>
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

BOOST_AUTO_TEST_CASE(block_db_test)
{
    block_header_type header0 = genesis_block().header;
    transaction_index_list txs0;
    const hash_digest h0 = hash_block_header(header0);

    block_header_type header1 = header0;
    header1.nonce = 4;
    transaction_index_list txs1{{110, 89, 76, 63, 44}};
    const hash_digest h1 = hash_block_header(header1);

    block_header_type header2 = header0;
    header2.nonce = 110;
    transaction_index_list txs2{{110, 89, 76, 63, 44}};
    const hash_digest h2 = hash_block_header(header2);

    block_header_type header3 = header0;
    header3.nonce = 88;
    transaction_index_list txs3{{63, 56, 89}};
    const hash_digest h3 = hash_block_header(header3);

    block_header_type header4a = header0;
    header4a.nonce = 63;
    transaction_index_list txs4a{{22, 12, 15, 77, 88, 99, 100}};
    const hash_digest h4a = hash_block_header(header4a);

    block_header_type header5a = header0;
    header5a.nonce = 26;
    transaction_index_list txs5a{{2, 4, 6, 8, 10, 12}};
    const hash_digest h5a = hash_block_header(header5a);

    block_header_type header4b = header0;
    header4b.nonce = 28;
    transaction_index_list txs4b{{100, 200, 500, 1000}};
    const hash_digest h4b = hash_block_header(header4b);

    block_header_type header5b = header0;
    header5b.nonce = 100;
    transaction_index_list txs5b{{88, 32, 12, 78, 100010810, 99}};
    const hash_digest h5b = hash_block_header(header5b);

    touch_file("block_db_lookup");
    touch_file("block_db_rows");
    block_database db("block_db_lookup", "block_db_rows");
    db.initialize_new();
    db.start();
    BOOST_REQUIRE(db.last_height() == block_database::null_height);
    db.store(header0, txs0);
    db.store(header1, txs1);
    db.store(header2, txs2);
    db.store(header3, txs3);
    BOOST_REQUIRE(db.last_height() == 3);
    // Fetch block 2 by hash.
    auto res_h2 = db.get(h2);
    BOOST_REQUIRE(res_h2);
    BOOST_REQUIRE(hash_block_header(res_h2.header()) == h2);
    for (size_t i = 0; i < res_h2.transactions_size(); ++i)
        BOOST_REQUIRE(res_h2.transaction_index(i) == txs2[i]);
    // Try a fork event.
    db.store(header4a, txs4a);
    db.store(header5a, txs5a);
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
    db.store(header4b, txs4b);
    db.store(header5b, txs5b);
    BOOST_REQUIRE(db.last_height() == 5);
    // Fetch blocks.
    auto res4b = db.get(4);
    BOOST_REQUIRE(res4b);
    BOOST_REQUIRE(hash_block_header(res4b.header()) == h4b);
    auto res5b = db.get(5);
    BOOST_REQUIRE(res5b);
    BOOST_REQUIRE(hash_block_header(res5b.header()) == h5b);
    for (size_t i = 0; i < res5b.transactions_size(); ++i)
        BOOST_REQUIRE(res5b.transaction_index(i) == txs5b[i]);
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
    touch_file("tx_db_index");
    transaction_database db("tx_db_map", "tx_db_index");
    db.initialize_new();
    db.start();
    index_type idx1 = db.store(info1, tx1);
    index_type idx2 = db.store(info2, tx2);
    BOOST_REQUIRE(idx1 == 0);
    BOOST_REQUIRE(idx2 == 1);
    auto res1 = db.get(h1);
    BOOST_REQUIRE(hash_transaction(res1.transaction()) == h1);
    auto res2 = db.get(h2);
    BOOST_REQUIRE(hash_transaction(res2.transaction()) == h2);
    auto res_idx2 = db.get(idx2);
    BOOST_REQUIRE(hash_transaction(res_idx2.transaction()) == h2);
    db.sync();
}

BOOST_AUTO_TEST_SUITE_END()

