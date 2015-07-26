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

using namespace libbitcoin;
using namespace libbitcoin::chain;

BOOST_AUTO_TEST_SUITE(disk_objs)

BOOST_AUTO_TEST_CASE(slab)
{
    touch_file("slabs");
    mmfile file("slabs");
    BITCOIN_ASSERT(file.data());
    file.resize(200);
    slab_allocator data(file, 0);
    data.create();

    data.start();

    position_type position = data.allocate(100);
    BOOST_REQUIRE(position == 8);
    //slab_type slab = data.get(position);

    position_type position2 = data.allocate(100);
    BOOST_REQUIRE(position2 == 108);
    //slab = data.get(position2);

    BOOST_REQUIRE(file.size() >= 208);
}

BOOST_AUTO_TEST_CASE(array)
{
    touch_file("array");
    mmfile file("array");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 4 * 10);

    disk_array<uint32_t, uint32_t> array(file, 0);
    array.create(10);
    array.start();

    array.write(9, 110);
    BOOST_REQUIRE(array.read(9) == 110);
}

BOOST_AUTO_TEST_CASE(record)
{
    touch_file("records");
    mmfile file("records");
    BITCOIN_ASSERT(file.data());
    file.resize(4);
    record_allocator recs(file, 0, 10);
    recs.create();

    recs.start();

    index_type idx = recs.allocate();
    BOOST_REQUIRE(idx == 0);
    idx = recs.allocate();
    BOOST_REQUIRE(idx == 1);
    BOOST_REQUIRE(file.size() >= 2 * 10 + 4);
    recs.sync();
}

BOOST_AUTO_TEST_CASE(linked_records_tst)
{
    touch_file("lrs");
    mmfile file("lrs");
    BITCOIN_ASSERT(file.data());
    file.resize(4);
    BC_CONSTEXPR size_t record_size = linked_record_offset + 6;
    record_allocator recs(file, 0, record_size);
    recs.create();

    recs.start();
    linked_records lrs(recs);

    index_type idx = lrs.create();
    BOOST_REQUIRE(idx == 0);
    index_type idx1 = lrs.create();
    BOOST_REQUIRE(idx1 == 1);
    idx = lrs.create();
    BOOST_REQUIRE(idx == 2);

    idx = lrs.insert(idx1);
    BOOST_REQUIRE(idx == 3);
    idx = lrs.insert(idx);
    BOOST_REQUIRE(idx == 4);

    size_t count = 0;
    index_type valid = idx;
    while (idx != linked_records::empty)
    {
        valid = idx;
        idx = lrs.next(idx);
        ++count;
    }
    BOOST_REQUIRE(count == 3);
    BOOST_REQUIRE(valid == idx1);
    recs.sync();
}

BOOST_AUTO_TEST_CASE(htdb_slab_tst)
{
    touch_file("htdb_slab");
    mmfile file("htdb_slab");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 8 * 100 + 8);

    htdb_slab_header header(file, 0);
    header.create(100);
    header.start();

    slab_allocator alloc(file, 4 + 8 * 100);
    alloc.create();
    alloc.start();

    typedef byte_array<4> tiny_hash;
    htdb_slab<tiny_hash> ht(header, alloc);

    auto write = [](uint8_t* data)
    {
        data[0] = 110;
        data[1] = 110;
        data[2] = 4;
        data[3] = 99;
    };
    ht.store(tiny_hash{{0xde, 0xad, 0xbe, 0xef}}, write, 8);
    slab_type slab = ht.get(tiny_hash{{0xde, 0xad, 0xbe, 0xef}});
    BOOST_REQUIRE(slab);
    BOOST_REQUIRE(slab[0] == 110);
    BOOST_REQUIRE(slab[1] == 110);
    BOOST_REQUIRE(slab[2] == 4);
    BOOST_REQUIRE(slab[3] == 99);
    slab = ht.get(tiny_hash{{0xde, 0xad, 0xbe, 0xee}});
    BOOST_REQUIRE(!slab);
}

BOOST_AUTO_TEST_SUITE_END()

