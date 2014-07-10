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
#include <bitcoin/blockchain/database/htdb_slab.hpp>

using namespace libbitcoin;
using namespace libbitcoin::chain;

constexpr size_t total_txs = 200;
constexpr size_t tx_size = 200;
constexpr size_t buckets = 100;

BOOST_AUTO_TEST_SUITE(htdb)

data_chunk generate_random_bytes(
    std::default_random_engine& engine, size_t size)
{
    data_chunk result(size);
    for (uint8_t& byte: result)
        byte = engine() % std::numeric_limits<uint8_t>::max();
    return result;
}

void write_data()
{
    touch_file("htdb_slabs");
    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 8 * buckets + 8);

    htdb_slab_header header(file, 0);
    header.initialize_new(buckets);
    header.start();

    slab_allocator alloc(file, 4 + 8 * buckets);
    alloc.initialize_new();
    alloc.start();

    htdb_slab<hash_digest> ht(header, alloc);

    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        data_chunk value = generate_random_bytes(engine, tx_size);
        hash_digest key = bitcoin_hash(value);
        auto write = [&value](uint8_t* data)
        {
            std::copy(value.begin(), value.end(), data);
        };
        ht.store(key, value.size(), write);
    }
}

BOOST_AUTO_TEST_CASE(htdb_slab_write_read)
{
    write_data();

    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());

    htdb_slab_header header(file, 0);
    header.start();

    BOOST_REQUIRE(header.size() == buckets);

    slab_allocator alloc(file, 4 + 8 * header.size());
    alloc.start();

    htdb_slab<hash_digest> ht(header, alloc);

    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        data_chunk value = generate_random_bytes(engine, tx_size);
        hash_digest key = bitcoin_hash(value);

        const slab_type slab = ht.get(key);
        BOOST_REQUIRE(slab);

        BOOST_REQUIRE(std::equal(value.begin(), value.end(), slab));
    }
}

BOOST_AUTO_TEST_SUITE_END()

