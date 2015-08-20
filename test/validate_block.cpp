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

BOOST_AUTO_TEST_SUITE(validate_block)

class validate_block_fixture
  : public bc::blockchain::validate_block
{
public:
    validate_block_fixture()
        : validate_block(0, chain::block(), config::checkpoint::list())
    {
    }

    validate_block_fixture(size_t height, const chain::block& block,
        const config::checkpoint::list& checks)
      : validate_block(height, block, checks)
    {
    }

    // pure virtuals (mocks)

    uint32_t previous_block_bits() const
    {
        return 0;
    }

    uint64_t actual_timespan(size_t interval) const
    {
        return 0;
    }

    uint64_t median_time_past() const
    {
        return 0;
    }

    bool transaction_exists(const hash_digest& tx_hash) const
    {
        return false;
    }

    bool is_output_spent(const chain::output_point& outpoint) const
    {
        return false;
    }

    bool fetch_transaction(chain::transaction& tx,
        size_t& previous_height, const hash_digest& tx_hash) const
    {
        return false;
    }

    bool is_output_spent(const chain::output_point& previous_output,
        size_t index_in_parent, size_t input_index) const
    {
        return false;
    }

    chain::block_header fetch_block(size_t fetch_height) const
    {
        return chain::block_header();
    }

    // protected virtuals (accessors)

    //uint32_t work_required() const
    //{
    //}

    boost::posix_time::ptime current_time() const
    {
        return validate_block::current_time();
    }

    //bool is_valid_time_stamp(uint32_t timestamp) const
    //{
    //}

    //bool is_spent_duplicate(const transaction_type& tx) const
    //{
    //}

    static bool is_distinct_tx_set(const chain::transaction::list& txs)
    {
        return validate_block::is_distinct_tx_set(txs);
    }
};

BOOST_FIXTURE_TEST_SUITE(validate_block__current_time, validate_block_fixture)

BOOST_AUTO_TEST_CASE(validate_block__current_time__always__does_not_throw)
{
    BOOST_REQUIRE_NO_THROW(current_time());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(validate_block__is_distinct_tx_set)

BOOST_AUTO_TEST_CASE(validate_block__is_distinct_tx_set__empty__true)
{
    BOOST_REQUIRE(validate_block_fixture::is_distinct_tx_set({}));
}

BOOST_AUTO_TEST_CASE(validate_block__is_distinct_tx_set__single__true)
{
    const chain::transaction tx1{ 1, 0 };
    BOOST_REQUIRE(validate_block_fixture::is_distinct_tx_set({ tx1 }));
}

BOOST_AUTO_TEST_CASE(validate_block__is_distinct_tx_set__duplicate__false)
{
    const chain::transaction tx1{ 1, 0 };
    const chain::transaction tx2{ 1, 0 };
    BOOST_REQUIRE(!validate_block_fixture::is_distinct_tx_set({ tx1, tx2 }));
}

BOOST_AUTO_TEST_CASE(validate_block__is_distinct_tx_set__distinct_by_version__true)
{
    const chain::transaction tx1{ 1, 0 };
    const chain::transaction tx2{ 2, 0 };
    const chain::transaction tx3{ 3, 0 };
    BOOST_REQUIRE(validate_block_fixture::is_distinct_tx_set({ tx1, tx2, tx3 }));
}

BOOST_AUTO_TEST_CASE(validate_block__is_distinct_tx_set__partialy_distinct_by_version__false)
{
    const chain::transaction tx1{ 1, 0 };
    const chain::transaction tx2{ 2, 0 };
    const chain::transaction tx3{ 2, 0 };
    BOOST_REQUIRE(!validate_block_fixture::is_distinct_tx_set({ tx1, tx2, tx3 }));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
