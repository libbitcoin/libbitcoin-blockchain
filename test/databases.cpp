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
    db.store(key4, val4);
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
    auto res4 = db.get(key4);
    BOOST_REQUIRE(res4);
    BOOST_REQUIRE(res4.hash() == val4.hash && res4.index() == val4.index);
    // Delete record.
    db.remove(key3);
    BOOST_REQUIRE(!db.get(key3));
}

BOOST_AUTO_TEST_SUITE_END()

