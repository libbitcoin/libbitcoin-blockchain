/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/test/unit_test.hpp>

#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(transaction_pool_tests)

BOOST_AUTO_TEST_CASE(transaction_pool__construct__foo__bar)
{
    // TODO
}

BOOST_AUTO_TEST_CASE(transaction_pool__add_unconfirmed_transactions__empty_list__noop)
{
    bc::blockchain::settings blockchain_settings;
    bc::settings bitcoin_settings;
    transaction_pool pool(blockchain_settings, bitcoin_settings);

    transaction_const_ptr_list txs;
    pool.add_unconfirmed_transactions(txs);
    BOOST_REQUIRE_EQUAL(true, true);
}

////BOOST_AUTO_TEST_CASE(transaction_pool__add_unconfirmed_transactions__empty_list__noop)
////{
////    settings blockchain_settings;
////    transaction_pool pool(blockchain_settings);
////
////    transaction_const_ptr_list txs;
////    pool.add_unconfirmed_transactions(txs);
////    BOOST_REQUIRE_EQUAL(true, true);
////}

BOOST_AUTO_TEST_SUITE_END()
