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
#include <vector>
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;

BOOST_AUTO_TEST_SUITE(transaction_pool_tests)

class threadpool_fixture
  : public threadpool
{
};

class blockchain_fixture
  : public blockchain
{
public:
    virtual bool start()
    {
        return false;
    }

    virtual bool stop()
    {
        return false;
    }

    virtual void store(const block_type& block,
        store_block_handler handle_store)
    {
    }

    virtual void import(const block_type& import_block,
        import_block_handler handle_import)
    {
    }

    virtual void fetch_block_header(uint64_t height,
        fetch_handler_block_header handle_fetch)
    {
    }

    virtual void fetch_block_header(const hash_digest& hash,
        fetch_handler_block_header handle_fetch)
    {
    }

    virtual void fetch_block_transaction_hashes(const hash_digest& hash,
        fetch_handler_block_transaction_hashes handle_fetch)
    {
    }

    virtual void fetch_block_height(const hash_digest& hash,
        fetch_handler_block_height handle_fetch)
    {
    }

    virtual void fetch_last_height(fetch_handler_last_height handle_fetch)
    {
    }

    virtual void fetch_transaction(const hash_digest& hash,
        fetch_handler_transaction handle_fetch)
    {
    }

    virtual void fetch_transaction_index(const hash_digest& hash,
        fetch_handler_transaction_index handle_fetch)
    {
    }

    virtual void fetch_spend(const output_point& outpoint,
        fetch_handler_spend handle_fetch)
    {
    }

    virtual void fetch_history(const payment_address& address,
        fetch_handler_history handle_fetch, const uint64_t limit=0,
        const uint64_t from_height=0)
    {
    }

    virtual void fetch_stealth(const binary_type& prefix,
        fetch_handler_stealth handle_fetch, uint64_t from_height=0)
    {
    }

    virtual void subscribe_reorganize(reorganize_handler handle_reorganize)
    {
    }
};

class transaction_pool_fixture
  : public transaction_pool
{
public:
    transaction_pool_fixture(threadpool& pool, blockchain& chain)
      : transaction_pool(pool, chain)
    {
    }

    transaction_pool_fixture(threadpool& pool, blockchain& chain,
        const pool_buffer& txs)
      : transaction_pool(pool, chain)
    {
        stopped_ = false;
        for (const auto& entry: txs)
            buffer_.push_back(entry);
    }

    // Test accesors.

    void delete_all(const std::error_code& ec)
    {
        transaction_pool::delete_all(ec);
    }

    void delete_package(const std::error_code& ec)
    {
        transaction_pool::delete_package(ec);
    }

    void delete_package(const hash_digest& tx_hash, const code& ec)
    {
        transaction_pool::delete_package(tx_hash, ec);
    }

    void delete_package(const transaction_type& tx, const code& ec)
    {
        transaction_pool::delete_package(tx, ec);
    }

    void delete_dependencies(const hash_digest& tx_hash, const code& ec)
    {
        transaction_pool::delete_dependencies(tx_hash, ec);
    }

    void delete_dependencies(const output_point& point, const code& ec)
    {
        transaction_pool::delete_dependencies(point, ec);
    }

    void delete_dependencies(input_comparison is_dependency, const code& ec)
    {
        transaction_pool::delete_dependencies(is_dependency, ec);
    }

    void delete_superseded(const blockchain::block_list& blocks)
    {
        transaction_pool::delete_superseded(blocks);
    }

    void delete_confirmed_in_blocks(const blockchain::block_list& blocks)
    {
        transaction_pool::delete_confirmed_in_blocks(blocks);
    }

    void delete_spent_in_blocks(const blockchain::block_list& blocks)
    {
        transaction_pool::delete_spent_in_blocks(blocks);
    }

    pool_buffer& transactions()
    {
        return buffer_;
    }

    void stopped(bool stop)
    {
        stopped_ = stop;
    }
};

#define DECLARE_TRANSACTION_POOL(pool) \
    threadpool_fixture memory_pool; \
    blockchain_fixture block_chain; \
    transaction_pool_fixture pool(memory_pool, block_chain)

#define DECLARE_TRANSACTION_POOL_TXS(pool, txs) \
    threadpool_fixture memory_pool; \
    blockchain_fixture block_chain; \
    transaction_pool_fixture pool(memory_pool, block_chain, txs)

#define DECLARE_TRANSACTION(number, code) \
    transaction_type tx##number; \
    tx##number.locktime = number; \
    const size_t tx##number##_id = number; \
    const auto hash##number = hash_transaction(tx##number); \
    const auto handle_confirm##number = [](const std::error_code& ec) \
    { \
        BOOST_CHECK_EQUAL(ec.value(), code); \
    }; \
    transaction_entry_info entry##number{ hash##number, tx##number, handle_confirm##number }

#define TX_ID_AT_POSITION(pool, position) \
    pool.transactions()[position].tx.locktime

BOOST_AUTO_TEST_CASE(transaction_pool__construct1__always__does_not_throw)
{
    threadpool_fixture pool;
    blockchain_fixture chain;
    BOOST_REQUIRE_NO_THROW(transaction_pool_fixture(pool, chain));
}

BOOST_AUTO_TEST_CASE(transaction_pool__construct2__zero__zero)
{
    pool_buffer transactions;
    DECLARE_TRANSACTION_POOL_TXS(mempool, transactions);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__construct2__one__one_destructor_callback)
{
    DECLARE_TRANSACTION(1, error::service_stopped);
    pool_buffer buffer(2);
    buffer.push_back(entry1);
    DECLARE_TRANSACTION_POOL_TXS(mempool, buffer);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_all__empty__empty)
{
    pool_buffer transactions(2);
    DECLARE_TRANSACTION_POOL_TXS(mempool, transactions);
    BOOST_REQUIRE(mempool.transactions().empty());
    mempool.delete_all(error::unknown);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_all__two__empty_expected_callbacks)
{
    DECLARE_TRANSACTION(1, error::network_unreachable);
    DECLARE_TRANSACTION(2, error::network_unreachable);
    pool_buffer buffer(2);
    buffer.push_back(entry1);
    buffer.push_back(entry2);
    DECLARE_TRANSACTION_POOL_TXS(mempool, buffer);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    mempool.delete_all(error::network_unreachable);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_all__stopped_two__empty_expected_callbacks)
{
    DECLARE_TRANSACTION(1, error::address_blocked);
    DECLARE_TRANSACTION(2, error::address_blocked);
    pool_buffer buffer(2);
    buffer.push_back(entry1);
    buffer.push_back(entry2);
    DECLARE_TRANSACTION_POOL_TXS(mempool, buffer);
    mempool.stopped(true);
    mempool.delete_all(error::address_blocked);
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package1__three__oldest_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(1, error::futuristic_timestamp);
    DECLARE_TRANSACTION(2, error::service_stopped);
    DECLARE_TRANSACTION(3, error::service_stopped);

    pool_buffer buffer(5);
    buffer.push_back(entry1);
    buffer.push_back(entry2);
    buffer.push_back(entry3);
    DECLARE_TRANSACTION_POOL_TXS(mempool, buffer);
    mempool.delete_package(error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_package2__three__match_removed_expected_callbacks)
{
    DECLARE_TRANSACTION(1, error::service_stopped);
    DECLARE_TRANSACTION(2, error::futuristic_timestamp);
    DECLARE_TRANSACTION(3, error::service_stopped);

    pool_buffer buffer(5);
    buffer.push_back(entry1);
    buffer.push_back(entry2);
    buffer.push_back(entry3);
    DECLARE_TRANSACTION_POOL_TXS(mempool, buffer);
    mempool.delete_package(hash2, error::futuristic_timestamp);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 0), tx1_id);
    BOOST_REQUIRE_EQUAL(TX_ID_AT_POSITION(mempool, 1), tx3_id);
}

BOOST_AUTO_TEST_SUITE_END()
