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
#include <bitcoin/blockchain/database/slab_allocator.hpp>
#include <bitcoin/blockchain/database/record_allocator.hpp>
#include <bitcoin/blockchain/database/disk_array.hpp>
#include <bitcoin/blockchain/database/linked_records.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

using namespace libbitcoin;
using namespace libbitcoin::chain;

BOOST_AUTO_TEST_SUITE(disk_objs_test)

BOOST_AUTO_TEST_CASE(slab)
{
    touch_file("slabs");
    mmfile file("slabs");
    BITCOIN_ASSERT(file.data());
    file.resize(200);
    slab_allocator data(file, 0);
    data.initialize_new();

    data.start();

    position_type position = data.allocate(100);
    BOOST_REQUIRE(position == 8);
    slab_type slab = data.get(position);

    slab_allocator::accessor_type position2 = data.allocate(100);
    BOOST_REQUIRE(position2 == 108);
    slab = data.get(position2);

    BOOST_REQUIRE(file.size() >= 208);
}

BOOST_AUTO_TEST_CASE(array)
{
    touch_file("array");
    mmfile file("array");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 4 * 10);

    disk_array<uint32_t, uint32_t> array(file, 0);
    array.initialize_new(10);
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
    recs.initialize_new();

    recs.start();

    index_type idx = recs.allocate();
    BOOST_REQUIRE(idx == 0);
    idx = recs.allocate();
    BOOST_REQUIRE(idx == 1);
    BOOST_REQUIRE(file.size() >= 2 * 10 + 4);
}

BOOST_AUTO_TEST_CASE(linked_records_tst)
{
    touch_file("lrs");
    mmfile file("lrs");
    BITCOIN_ASSERT(file.data());
    file.resize(4);
    record_allocator recs(file, 0, 10);
    recs.initialize_new();

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
}

BOOST_AUTO_TEST_SUITE_END()

