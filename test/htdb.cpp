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
#include <random>
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain.hpp>

using namespace libbitcoin;
using namespace libbitcoin::blockchain;

BC_CONSTEXPR size_t total_txs = 200;
BC_CONSTEXPR size_t tx_size = 200;
BC_CONSTEXPR size_t buckets = 100;

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
    BC_CONSTEXPR size_t header_size = htdb_slab_header_fsize(buckets);

    database::touch_file("htdb_slabs");
    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());
    file.resize(header_size + min_slab_fsize);

    htdb_slab_header header(file, 0);
    header.create(buckets);
    header.start();

    const position_type slab_start = header_size;

    slab_allocator alloc(file, slab_start);
    alloc.create();
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
        ht.store(key, write, value.size());
    }

    alloc.sync();
}

BOOST_AUTO_TEST_CASE(htdb_slab_write_read)
{
    write_data();

    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());

    htdb_slab_header header(file, 0);
    header.start();

    BOOST_REQUIRE(header.size() == buckets);

    const position_type slab_start = htdb_slab_header_fsize(buckets);

    slab_allocator alloc(file, slab_start);
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

BOOST_AUTO_TEST_CASE(htdb_record_test_32)
{
    BC_CONSTEXPR size_t rec_buckets = 2;
    BC_CONSTEXPR size_t header_size = htdb_record_header_fsize(rec_buckets);

    database::touch_file("htdb_records");
    mmfile file("htdb_records");
    BITCOIN_ASSERT(file.data());
    file.resize(header_size + min_records_fsize);

    htdb_record_header header(file, 0);
    header.create(rec_buckets);
    header.start();

    typedef byte_array<4> tiny_hash;
    BC_CONSTEXPR size_t record_size = record_fsize_htdb<tiny_hash>(4);
    const position_type records_start = header_size;

    record_allocator alloc(file, records_start, record_size);
    alloc.create();
    alloc.start();

    htdb_record<tiny_hash> ht(header, alloc, "test");

    tiny_hash key{{0xde, 0xad, 0xbe, 0xef}};
    auto write = [](uint8_t* data)
    {
        data[0] = 110;
        data[1] = 110;
        data[2] = 4;
        data[3] = 88;
    };
    ht.store(key, write);

    tiny_hash key1{{0xb0, 0x0b, 0xb0, 0x0b}};
    auto write1 = [](uint8_t* data)
    {
        data[0] = 99;
        data[1] = 98;
        data[2] = 97;
        data[3] = 96;
    };
    ht.store(key, write);
    ht.store(key1, write1);
    ht.store(key1, write);

    alloc.sync();

    BOOST_REQUIRE(header.read(0) == header.empty);
    BOOST_REQUIRE(header.read(1) == 3);

    htdb_record_list_item<tiny_hash> item(alloc, 3);
    BOOST_REQUIRE(item.next_index() == 2);
    htdb_record_list_item<tiny_hash> item1(alloc, 2);
    BOOST_REQUIRE(item1.next_index() == 1);

    // Should unlink record 1
    BOOST_REQUIRE(ht.unlink(key));

    BOOST_REQUIRE(header.read(1) == 3);
    htdb_record_list_item<tiny_hash> item2(alloc, 2);
    BOOST_REQUIRE(item2.next_index() == 0);

    // Should unlink record 3 from buckets
    BOOST_REQUIRE(ht.unlink(key1));

    BOOST_REQUIRE(header.read(1) == 2);

    tiny_hash invalid{{0x00, 0x01, 0x02, 0x03}};
    BOOST_REQUIRE(!ht.unlink(invalid));
}

BOOST_AUTO_TEST_CASE(htdb_record_test_64)
{
    BC_CONSTEXPR size_t rec_buckets = 2;
    BC_CONSTEXPR size_t header_size = htdb_record_header_fsize(rec_buckets);

    database::touch_file("htdb_records");
    mmfile file("htdb_records");
    BITCOIN_ASSERT(file.data());
    file.resize(header_size + min_records_fsize);

    htdb_record_header header(file, 0);
    header.create(rec_buckets);
    header.start();

    typedef byte_array<8> tiny_hash;
    BC_CONSTEXPR size_t record_size = record_fsize_htdb<tiny_hash>(8);
    const position_type records_start = header_size;

    record_allocator alloc(file, records_start, record_size);
    alloc.create();
    alloc.start();

    htdb_record<tiny_hash> ht(header, alloc, "test");

    tiny_hash key{{0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef}};
    auto write = [](uint8_t* data)
    {
        data[0] = 110;
        data[1] = 110;
        data[2] = 4;
        data[3] = 88;
        data[4] = 110;
        data[5] = 110;
        data[6] = 4;
        data[7] = 88;
    };
    ht.store(key, write);

    tiny_hash key1{{0xb0, 0x0b, 0xb0, 0x0b, 0xb0, 0x0b, 0xb0, 0x0b}};
    auto write1 = [](uint8_t* data)
    {
        data[0] = 99;
        data[1] = 98;
        data[2] = 97;
        data[3] = 96;
        data[4] = 95;
        data[5] = 94;
        data[6] = 93;
        data[7] = 92;
    };
    ht.store(key, write);
    ht.store(key1, write1);
    ht.store(key1, write);

    alloc.sync();

    BOOST_REQUIRE(header.read(0) == header.empty);
    BOOST_REQUIRE(header.read(1) == 3);

    htdb_record_list_item<tiny_hash> item(alloc, 3);
    BOOST_REQUIRE(item.next_index() == 2);
    htdb_record_list_item<tiny_hash> item1(alloc, 2);
    BOOST_REQUIRE(item1.next_index() == 1);

    // Should unlink record 1
    BOOST_REQUIRE(ht.unlink(key));

    BOOST_REQUIRE(header.read(1) == 3);
    htdb_record_list_item<tiny_hash> item2(alloc, 2);
    BOOST_REQUIRE(item2.next_index() == 0);

    // Should unlink record 3 from buckets
    BOOST_REQUIRE(ht.unlink(key1));

    BOOST_REQUIRE(header.read(1) == 2);

    tiny_hash invalid{{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}};
    BOOST_REQUIRE(!ht.unlink(invalid));
}

BOOST_AUTO_TEST_SUITE_END()

