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

    void delete_all()
    {
        transaction_pool::delete_all();
    }

    void delete_package(const std::error_code& ec)
    {
        transaction_pool::delete_package(ec);
    }

    void delete_package(const hash_digest& tx_hash, const code& ec)
    {
        transaction_pool::delete_package(ec);
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

BOOST_AUTO_TEST_CASE(transaction_pool__construct2__one__one)
{
    pool_buffer transactions(2);
    transactions.push_back(transaction_entry_info());
    DECLARE_TRANSACTION_POOL_TXS(mempool, transactions);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 1u);
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_all__empty__empty)
{
    pool_buffer transactions(2);
    DECLARE_TRANSACTION_POOL_TXS(mempool, transactions);
    BOOST_REQUIRE(mempool.transactions().empty());
    mempool.delete_all();
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_all__two__expected_handler_calls)
{
    const auto handle_confirm1 = [](const std::error_code& ec)
    {
        BOOST_REQUIRE_EQUAL(ec.value(), error::blockchain_reorganized);
    };

    const auto handle_confirm2 = [](const std::error_code& ec)
    {
        BOOST_REQUIRE_EQUAL(ec.value(), error::blockchain_reorganized);
    };

    transaction_type tx;
    hash_digest hash(null_hash);
    transaction_entry_info entry1{ hash, tx, handle_confirm1 };
    transaction_entry_info entry2{ hash, tx, handle_confirm2 };

    pool_buffer transactions(2);
    transactions.push_back(entry1);
    transactions.push_back(entry2);
    DECLARE_TRANSACTION_POOL_TXS(mempool, transactions);
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
    mempool.delete_all();
    BOOST_REQUIRE(mempool.transactions().empty());
}

BOOST_AUTO_TEST_CASE(transaction_pool__delete_all__stopped_two__no_handler_calls_two)
{
    const auto handle_confirm = [](const std::error_code& ec)
    {
        BOOST_REQUIRE(false);
    };

    transaction_type tx;
    hash_digest hash(null_hash);
    transaction_entry_info entry{ hash, tx, handle_confirm };

    pool_buffer transactions(2);
    transactions.push_back(entry);
    transactions.push_back(entry);
    DECLARE_TRANSACTION_POOL_TXS(mempool, transactions);
    mempool.stopped(true);
    mempool.delete_all();
    BOOST_REQUIRE_EQUAL(mempool.transactions().size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()
