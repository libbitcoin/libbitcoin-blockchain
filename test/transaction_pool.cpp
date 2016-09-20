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
#include <memory>
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::blockchain;

BOOST_AUTO_TEST_SUITE(transaction_pool_tests)

class threadpool_fixture
  : public threadpool
{
};

class blockchain_fixture
  : public full_chain
{
public:

    // Start/Stop.
    //-------------------------------------------------------------------------

    virtual bool start()
    {
        return false;
    }

    virtual bool stop()
    {
        return false;
    }

    virtual bool close()
    {
        return false;
    }

    // Fetch.
    //-------------------------------------------------------------------------

    virtual void fetch_block(uint64_t height,
        block_fetch_handler handler) const
    {
    }

    virtual void fetch_block(const hash_digest& hash,
        block_fetch_handler handler) const
    {
    }

    virtual void fetch_block_header(uint64_t height,
        block_header_fetch_handler handler) const
    {
    }

    virtual void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler) const
    {
    }

    virtual void fetch_block_transaction_hashes(uint64_t height,
        transaction_hashes_fetch_handler handler) const
    {
    }

    virtual void fetch_block_transaction_hashes(const hash_digest& hash,
        transaction_hashes_fetch_handler handler) const
    {
    }

    virtual void fetch_block_locator(
        block_locator_fetch_handler handle_fetch) const
    {
    }

    virtual void fetch_locator_block_hashes(get_blocks_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_hashes_fetch_handler handler) const
    {
    }

    virtual void fetch_locator_block_headers(get_headers_const_ptr locator,
        const hash_digest& threshold, size_t limit,
        locator_block_headers_fetch_handler handler) const
    {
    }

    virtual void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler) const
    {
    }

    virtual void fetch_last_height(last_height_fetch_handler handler) const
    {
    }

    virtual void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handler) const
    {
    }

    virtual void fetch_transaction_index(const hash_digest& hash,
        transaction_index_fetch_handler handler) const
    {
    }

    virtual void fetch_spend(const output_point& outpoint,
        spend_fetch_handler handler) const
    {
    }

    virtual void fetch_history(const wallet::payment_address& address,
        uint64_t limit, uint64_t from_height,
        history_fetch_handler handler) const
    {
    }

    // Filters.
    //-------------------------------------------------------------------------

    virtual void fetch_stealth(const binary& prefix, uint64_t from_height,
        stealth_fetch_handler handler) const
    {
    }

    virtual void filter_blocks(get_data_ptr message,
        result_handler handler) const
    {
    }

    virtual void filter_transactions(get_data_ptr message,
        result_handler handler) const
    {
    }

    virtual void filter_orphans(get_data_ptr message,
        result_handler handler) const
    {
    }

    virtual void filter_floaters(get_data_ptr message,
        result_handler handler) const
    {
    }

    // Subscribers.
    //-------------------------------------------------------------------------

    virtual void subscribe_reorganize(organizer::reorganize_handler handler)
    {
    }

    virtual void subscribe_transaction(transaction_handler handler)
    {
    }

    // Stores.
    //-------------------------------------------------------------------------

    virtual void store(block_const_ptr block, block_store_handler handler)
    {
    }

    virtual void store(transaction_const_ptr block,
        transaction_store_handler handler)
    {
    }
};

class transaction_pool_fixture
  : public transaction_pool
{
public:
    typedef transaction_pool::buffer buffer;

    static blockchain::settings settings_factory(size_t capacity,
        bool consistency)
    {
        blockchain::settings value;
        value.transaction_pool_capacity = capacity;
        value.transaction_pool_consistency = consistency;
        return value;
    }

    transaction_pool_fixture(threadpool& pool, full_chain& chain,
        const blockchain::settings& settings)
      : transaction_pool(pool, chain, settings)
    {
    }

    transaction_pool_fixture(threadpool& pool, full_chain& chain, buffer& txs)
      : transaction_pool(pool, chain, settings_factory(txs.capacity(), true))
    {
        // Start by default, fill with our test buffer data.
        stopped_ = false;
        for (const auto tx: txs)
            buffer_.push_back(tx);
    }

    // Test accesors.
    //-------------------------------------------------------------------------

    void add(transaction_const_ptr tx, result_handler handler)
    {
        transaction_pool::add(tx, handler);
    }

    void remove(const block_const_ptr_list& blocks)
    {
        transaction_pool::remove(blocks);
    }

    void clear(const code& ec)
    {
        transaction_pool::clear(ec);
    }

    void delete_spent_in_blocks(const block_const_ptr_list& blocks)
    {
        transaction_pool::delete_spent_in_blocks(blocks);
    }

    void delete_confirmed_in_blocks(const block_const_ptr_list& blocks)
    {
        transaction_pool::delete_confirmed_in_blocks(blocks);
    }

    void delete_dependencies(const hash_digest& tx_hash, const code& ec)
    {
        transaction_pool::delete_dependencies(tx_hash, ec);
    }

    void delete_dependencies(const output_point& point, const code& ec)
    {
        transaction_pool::delete_dependencies(point, ec);
    }

    void delete_dependencies(input_compare is_dependency, const code& ec)
    {
        transaction_pool::delete_dependencies(is_dependency, ec);
    }

    void delete_package(const code& ec)
    {
        transaction_pool::delete_package(ec);
    }

    void delete_package(transaction_const_ptr tx, const code& ec)
    {
        transaction_pool::delete_package(tx, ec);
    }

    void delete_single(const hash_digest& tx_hash, const code& ec)
    {
        transaction_pool::delete_single(tx_hash, ec);
    }

    // Test
    //-------------------------------------------------------------------------

    const buffer& transactions()
    {
        return buffer_;
    }

    void stopped(bool stop)
    {
        stopped_ = stop;
    }
};

#define DECLARE_TRANSACTION_POOL(pool, txs) \
    threadpool_fixture memory_pool_; \
    blockchain_fixture full_chain_; \
    transaction_pool_fixture pool(memory_pool_, full_chain_, txs)

#define DECLARE_TRANSACTION(number, code_) \
    auto tx##number = std::make_shared<message::transaction_message>(); \
    tx##number->locktime = number; \
    auto hash##number = tx##number->hash(); \
    const size_t tx##number##_id = number; \
    code result##number(error::unknown); \
    const auto handle_confirm##number = [&result##number](const code& ec) \
    { \
        result##number = ec; \
        BOOST_CHECK_EQUAL(ec.value(), code_); \
    }; \
    tx##number->metadata.confirm = handle_confirm##number;

#define REQUIRE_CALLBACK(number, code) \
    BOOST_CHECK_EQUAL(result##number, code)

#define TX_ID_AT_POSITION(pool, position) \
    pool.transactions()[position]->locktime

#define ADD_INPUT_TO_TX_NUMBER(number, prevout_hash, prevout_index) \
    tx##number->inputs.push_back({ { prevout_hash, prevout_index }, {}, 0 }); \
    hash##number = tx##number->hash();

BOOST_AUTO_TEST_SUITE(transaction_pool__construct)

BOOST_AUTO_TEST_CASE(transaction_pool__construct1__always__does_not_throw)
{
    threadpool_fixture pool;
    blockchain_fixture chain;
    BOOST_REQUIRE_NO_THROW(transaction_pool_fixture(pool, chain, {}));
}

BOOST_AUTO_TEST_CASE(transaction_pool__construct2__zero__zero)
{
    transaction_pool_fixture::buffer buffer;
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__construct2__one__one_destructor_callback)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(2);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__add__zero_capacity__empty)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.add(tx0, handle_confirm0);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__add__empty__one)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.add(tx0, handle_confirm0);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__add__overflow_two__oldest_two_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::pool_filled);
    DECLARE_TRANSACTION(1, error::pool_filled);
    DECLARE_TRANSACTION(2, error::service_stopped);
    transaction_pool_fixture::buffer buffer(1);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.add(tx0, handle_confirm0);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    mempool.add(tx1, handle_confirm1);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx1_id);
    mempool.add(tx2, handle_confirm2);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__add__overflow_with_dependencies__removes_oldest_and_dependencies)
{
    DECLARE_TRANSACTION(0, error::pool_filled);
    DECLARE_TRANSACTION(1, error::pool_filled);
    DECLARE_TRANSACTION(2, error::pool_filled);
    DECLARE_TRANSACTION(3, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(3);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.add(tx3, handle_confirm3);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx3_id);
    REQUIRE_CALLBACK(0, error::pool_filled);
    REQUIRE_CALLBACK(1, error::pool_filled);
    REQUIRE_CALLBACK(2, error::pool_filled);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(transaction_pool__clear)

BOOST_AUTO_TEST_CASE(transaction_pool__clear__empty__empty)
{
    transaction_pool_fixture::buffer buffer(2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.clear(error::unknown);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__clear__two__empty_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::network_unreachable);
    DECLARE_TRANSACTION(1, error::network_unreachable);
    transaction_pool_fixture::buffer buffer(2);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    mempool.clear(error::network_unreachable);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__clear__stopped_two__empty_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::address_blocked);
    DECLARE_TRANSACTION(1, error::address_blocked);
    transaction_pool_fixture::buffer buffer(2);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.clear(error::address_blocked);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package1__empty__empty)
{
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package1__three__oldest_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package1__stopped__unchanged_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_package(error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package1__dependencies__deletes_self_and_dependencies)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
    REQUIRE_CALLBACK(2, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package2__empty__empty)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx0, error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package2__three__match_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx1, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package2__no_match__no_change_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    DECLARE_TRANSACTION(3, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx3, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 3u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 2), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package2__stopped__unchanged_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_package(tx0, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package2__dependencies__deletes_self_and_dependencies)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx0, error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
    REQUIRE_CALLBACK(2, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package3__empty__empty)
{
    DECLARE_TRANSACTION(0, 42);
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx0, error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package3__three__match_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx1, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package3__no_match__no_change_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    DECLARE_TRANSACTION(3, 42);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx3, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 3u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 2), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package3__stopped__unchanged_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_package(tx0, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package3__dependencies__deletes_self_and_dependencies)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_package(tx0, error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
    REQUIRE_CALLBACK(2, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(transaction_pool__delete_single)

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single1__empty__empty)
{
    DECLARE_TRANSACTION(0, 42);
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_single(hash0, error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single1__three__match_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_single(hash1, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single__no_match__no_change_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    DECLARE_TRANSACTION(3, 42);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_single(hash3, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 3u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 2), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single__stopped__unchanged_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_single(hash0, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single__dependencies__deletes_self_only)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_single(hash0, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single__empty__empty)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_single(hash0, error::futuristic_timestamp);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_single__three__match_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);

    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_single(hash1, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(transaction_pool__delete_dependencies)

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies1__empty__empty)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(output_point{ hash0, 0 }, error::unknown);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies1__forward_full_chain__head_remains)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(output_point{ hash0, 42 }, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
    REQUIRE_CALLBACK(2, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies1__reverse_full_chain__head_remains)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash2, 24);
    ADD_INPUT_TO_TX_NUMBER(0, hash1, 42);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(output_point{ hash2, 24 }, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx2_id);
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies1__partial_chain__expected)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash2, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(output_point{ hash2, 24 }, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies1__multiple_chlidren__removed_by_index)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 24);
    ADD_INPUT_TO_TX_NUMBER(2, hash0, 42);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(output_point{ hash0, 24 }, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies1__stopped_full_chain__unchanged)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_dependencies(output_point{ hash0, 42 }, error::unknown);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 3u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 2), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies2__empty__empty)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    transaction_pool_fixture::buffer buffer(0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(hash0, error::unknown);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies2__forward_full_chain__head_remains)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(hash0, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
    REQUIRE_CALLBACK(2, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies2__reverse_full_chain__head_remains)
{
    DECLARE_TRANSACTION(0, error::futuristic_timestamp);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash2, 24);
    ADD_INPUT_TO_TX_NUMBER(0, hash1, 42);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(hash2, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx2_id);
    REQUIRE_CALLBACK(0, error::futuristic_timestamp);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies2__partial_chain__expected)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash2, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(hash2, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies2__multiple_chlidren__all_removed)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 24);
    ADD_INPUT_TO_TX_NUMBER(2, hash0, 42);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(hash0, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies2__stopped_full_chain__unchanged)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_dependencies(hash0, error::unknown);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 3u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 2), tx2_id);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_dependencies3__custom_comparison_partial_chain__expected)
{
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(2, hash0, 42);
    ADD_INPUT_TO_TX_NUMBER(1, hash2, 24);
    transaction_pool_fixture::buffer buffer(5);
    buffer.push_back(tx0);
    buffer.push_back(tx1);
    buffer.push_back(tx2);

    // ONLY match hash1 (so tx2 will not be deleted).
    const auto comparison = [&hash1](const chain::input& input)
    {
        return input.previous_output.hash == hash1;
    };

    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_dependencies(hash2, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx0_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx2_id);
    REQUIRE_CALLBACK(1, error::futuristic_timestamp);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(transaction_pool__delete_confirmed_in_blocks)

BOOST_AUTO_TEST_CASE(transaction_pool__delete_confirmed_in_blocks__empty_block__expected)
{
    block_const_ptr_list blocks;
    block block1;
    blocks.push_back(std::make_shared<const message::block_message>(block1));
    transaction_pool_fixture::buffer buffer(1);
    DECLARE_TRANSACTION(0, error::service_stopped);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_confirmed_in_blocks(blocks);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_confirmed_in_blocks__one_block_no_dependencies__expected)
{
    block_const_ptr_list blocks;
    block block1;
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::success);
    DECLARE_TRANSACTION(2, error::success);
    DECLARE_TRANSACTION(3, error::service_stopped);
    block1.transactions.push_back(*tx0);
    block1.transactions.push_back(*tx1);
    block1.transactions.push_back(*tx2);
    block1.transactions.push_back(*tx3);
    blocks.push_back(std::make_shared<const message::block_message>(block1));
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION(4, error::service_stopped);
    buffer.push_back(tx2);
    buffer.push_back(tx4);
    buffer.push_back(tx1);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_confirmed_in_blocks(blocks);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx4_id);
    REQUIRE_CALLBACK(1, error::success);
    REQUIRE_CALLBACK(2, error::success);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_confirmed_in_blocks__two_blocks_dependencies__expected)
{
    block_const_ptr_list blocks;
    block block1;
    block block2;
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::success);
    DECLARE_TRANSACTION(2, error::success);
    block1.transactions.push_back(*tx0);
    block2.transactions.push_back(*tx1);
    block2.transactions.push_back(*tx2);
    blocks.push_back(std::make_shared<const message::block_message>(block1));
    blocks.push_back(std::make_shared<const message::block_message>(block2));
    transaction_pool_fixture::buffer buffer(8);
    DECLARE_TRANSACTION(3, error::service_stopped);
    DECLARE_TRANSACTION(4, error::service_stopped);
    DECLARE_TRANSACTION(5, error::service_stopped);
    DECLARE_TRANSACTION(6, error::service_stopped);
    DECLARE_TRANSACTION(7, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(3, hash1, 42);
    ADD_INPUT_TO_TX_NUMBER(4, hash3, 43);
    ADD_INPUT_TO_TX_NUMBER(5, hash4, 44);
    ADD_INPUT_TO_TX_NUMBER(6, hash4, 45);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    buffer.push_back(tx3);
    buffer.push_back(tx4);
    buffer.push_back(tx5);
    buffer.push_back(tx6);
    buffer.push_back(tx7);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_confirmed_in_blocks(blocks);
    BOOST_CHECK_EQUAL(mempool.transactions().size(), 5u);
    BOOST_CHECK_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx3_id);
    BOOST_CHECK_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx4_id);
    BOOST_CHECK_EQUAL(TX_ID_AT_POSITION(mempool, 2), tx5_id);
    BOOST_CHECK_EQUAL(TX_ID_AT_POSITION(mempool, 3), tx6_id);
    BOOST_CHECK_EQUAL(TX_ID_AT_POSITION(mempool, 4), tx7_id);
    REQUIRE_CALLBACK(1, error::success);
    REQUIRE_CALLBACK(2, error::success);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(transaction_pool__delete_spent_in_blocks)

BOOST_AUTO_TEST_CASE(transaction_pool__delete_spent_in_blocks__empty_block__expected)
{
    block_const_ptr_list blocks;
    block block1;
    blocks.push_back(std::make_shared<message::block_message>(block1));
    transaction_pool_fixture::buffer buffer(1);
    DECLARE_TRANSACTION(0, error::service_stopped);
    buffer.push_back(tx0);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_spent_in_blocks(blocks);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_spent_in_blocks__two_blocks_no_duplicates_or_dependencies__expected)
{
    block_const_ptr_list blocks;
    block block1;
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    DECLARE_TRANSACTION(3, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(0, null_hash, 42);
    ADD_INPUT_TO_TX_NUMBER(1, null_hash, 43);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 44);
    ADD_INPUT_TO_TX_NUMBER(2, null_hash, 45);
    ADD_INPUT_TO_TX_NUMBER(2, hash0, 46);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 47);
    ADD_INPUT_TO_TX_NUMBER(3, null_hash, 48);
    ADD_INPUT_TO_TX_NUMBER(3, hash0, 49);
    ADD_INPUT_TO_TX_NUMBER(3, hash1, 59);
    ADD_INPUT_TO_TX_NUMBER(3, hash2, 51);
    block1.transactions.push_back(*tx0);
    block1.transactions.push_back(*tx1);
    block1.transactions.push_back(*tx2);
    block1.transactions.push_back(*tx3);
    blocks.push_back(std::make_shared<const message::block_message>(block1));
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION(4, error::double_spend);
    DECLARE_TRANSACTION(5, error::service_stopped);
    DECLARE_TRANSACTION(6, error::service_stopped);
    DECLARE_TRANSACTION(7, error::double_spend);
    ADD_INPUT_TO_TX_NUMBER(4, hash0, 46);
    ADD_INPUT_TO_TX_NUMBER(5, hash1, 99);
    ADD_INPUT_TO_TX_NUMBER(6, hash2, 99);
    ADD_INPUT_TO_TX_NUMBER(7, hash2, 51);
    buffer.push_back(tx4);
    buffer.push_back(tx5);
    buffer.push_back(tx6);
    buffer.push_back(tx7);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.delete_spent_in_blocks(blocks);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx5_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx6_id);
    REQUIRE_CALLBACK(4, error::double_spend);
    REQUIRE_CALLBACK(7, error::double_spend);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(transaction_pool__remove)

BOOST_AUTO_TEST_CASE(transaction_pool__remove__one_block_duplicates_no_spends__removed_as_succeeded)
{
    block_const_ptr_list blocks;
    block block1;
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::success);
    DECLARE_TRANSACTION(2, error::success);
    DECLARE_TRANSACTION(3, error::success);
    block1.transactions.push_back(*tx0);
    block1.transactions.push_back(*tx1);
    block1.transactions.push_back(*tx2);
    block1.transactions.push_back(*tx3);
    blocks.push_back(std::make_shared<const message::block_message>(block1));
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION(4, error::service_stopped);
    DECLARE_TRANSACTION(5, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(4, hash3, 42);
    buffer.push_back(tx1);
    buffer.push_back(tx2);
    buffer.push_back(tx3);
    buffer.push_back(tx4);
    buffer.push_back(tx5);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.remove(blocks);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx4_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx5_id);
    REQUIRE_CALLBACK(1, error::success);
    REQUIRE_CALLBACK(2, error::success);
    REQUIRE_CALLBACK(3, error::success);
}

BOOST_AUTO_TEST_CASE(transaction_pool__remove__two_blocks_spends_no_duplicates__removed_as_spent)
{
    block_const_ptr_list blocks;
    block block1;
    DECLARE_TRANSACTION(0, error::service_stopped);
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::service_stopped);
    DECLARE_TRANSACTION(3, error::service_stopped);
    ADD_INPUT_TO_TX_NUMBER(0, null_hash, 42);
    ADD_INPUT_TO_TX_NUMBER(1, null_hash, 43);
    ADD_INPUT_TO_TX_NUMBER(1, hash0, 44);
    ADD_INPUT_TO_TX_NUMBER(2, null_hash, 45);
    ADD_INPUT_TO_TX_NUMBER(2, hash0, 46);
    ADD_INPUT_TO_TX_NUMBER(2, hash1, 47);
    ADD_INPUT_TO_TX_NUMBER(3, null_hash, 48);
    ADD_INPUT_TO_TX_NUMBER(3, hash0, 49);
    ADD_INPUT_TO_TX_NUMBER(3, hash1, 59);
    ADD_INPUT_TO_TX_NUMBER(3, hash2, 51);
    block1.transactions.push_back(*tx0);
    block1.transactions.push_back(*tx1);
    block1.transactions.push_back(*tx2);
    block1.transactions.push_back(*tx3);
    blocks.push_back(std::make_shared<const message::block_message>(block1));
    transaction_pool_fixture::buffer buffer(5);
    DECLARE_TRANSACTION(4, error::double_spend);
    DECLARE_TRANSACTION(5, error::double_spend);
    DECLARE_TRANSACTION(6, error::service_stopped);
    DECLARE_TRANSACTION(7, error::double_spend);
    ADD_INPUT_TO_TX_NUMBER(4, hash0, 46);
    ADD_INPUT_TO_TX_NUMBER(5, hash4, 99);
    ADD_INPUT_TO_TX_NUMBER(6, hash2, 99);
    ADD_INPUT_TO_TX_NUMBER(7, hash2, 51);
    buffer.push_back(tx4);
    buffer.push_back(tx5);
    buffer.push_back(tx6);
    buffer.push_back(tx7);
    DECLARE_TRANSACTION_POOL(mempool, buffer);
    mempool.remove(blocks);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx6_id);
    REQUIRE_CALLBACK(4, error::double_spend);
    REQUIRE_CALLBACK(5, error::double_spend);
    REQUIRE_CALLBACK(7, error::double_spend);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
