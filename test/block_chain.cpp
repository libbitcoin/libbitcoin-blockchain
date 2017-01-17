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
#include <boost/test/unit_test.hpp>

#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(block_chain_tests)

////block_const_ptr make_block(uint32_t id, size_t height,
////    const hash_digest& parent)
////{
////    const auto block = std::make_shared<const message::block>(message::block
////    {
////        chain::header{ id, parent, null_hash, 0, 0, 0 }, {}
////    });
////
////    block->header().validation.height = height;
////    return block;
////}
////
////block_const_ptr make_block(uint32_t id, size_t height, block_const_ptr parent)
////{
////    return make_block(id, height, parent->hash());
////}
////
////block_const_ptr make_block(uint32_t id, size_t height)
////{
////    return make_block(id, height, null_hash);
////}

// fetch_block1 (by height)

BOOST_AUTO_TEST_CASE(block_chain__fetch_block1__foo__bar)
{
    // TODO
}

// fetch_block1 (by hash)

BOOST_AUTO_TEST_CASE(block_chain__fetch_block2__foo__bar)
{
    // TODO
}

BOOST_AUTO_TEST_SUITE_END()
