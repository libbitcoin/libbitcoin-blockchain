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

#include <future>
#include <bitcoin/blockchain.hpp>
#include "utility.hpp"

using namespace bc;
using namespace bc::blockchain;
using namespace bc::database;

#define TEST_SET_NAME \
    "safe_chain_tests"

class safe_chain_setup_fixture
{
public:
    safe_chain_setup_fixture()
    {
        log::initialize();
    }
};

BOOST_FIXTURE_TEST_SUITE(safe_chain_tests, safe_chain_setup_fixture)

// fetch_block

static int fetch_block_by_height_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, block_const_ptr result_block,
        size_t result_height)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        const auto match = result_height == height && *result_block == *block;
        promise.set_value(match ? error::success : error::operation_failed);
    };
    instance.fetch_block(height, true, handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block1__unstarted__error_service_stopped)
{
    threadpool pool;
    database::settings database_settings;
    database_settings.directory = TEST_NAME;
    BOOST_REQUIRE(test::create_database(database_settings));

    blockchain::settings blockchain_settings;
    block_chain instance(pool, blockchain_settings, database_settings,
        bc::settings());

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_block_by_height_result(instance, block1, 1), error::service_stopped);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block1__exists__success)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE_EQUAL(fetch_block_by_height_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block1__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_block_by_height_result(instance, block1, 1), error::not_found);
}

static int fetch_block_by_hash_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, block_const_ptr result_block,
        size_t result_height)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        const auto match = result_height == height && *result_block == *block;
        promise.set_value(match ? error::success : error::operation_failed);
    };
    instance.fetch_block(block->hash(), true, handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block2__exists__success)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE_EQUAL(fetch_block_by_hash_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block2__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_block_by_hash_result(instance, block1, 1), error::not_found);
}

// fetch_block_header

static int fetch_block_header_by_height_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, header_ptr result_header,
        size_t result_height)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        const auto match = result_height == height &&
            *result_header == block->header();
        promise.set_value(match ? error::success : error::operation_failed);
    };
    instance.fetch_block_header(height, handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block_header1__exists__success)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE_EQUAL(fetch_block_header_by_height_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block_header1__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_block_header_by_height_result(instance, block1, 1), error::not_found);
}

static int fetch_block_header_by_hash_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, header_ptr result_header,
        size_t result_height)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        const auto match = result_height == height &&
            *result_header == block->header();
        promise.set_value(match ? error::success : error::operation_failed);
    };
    instance.fetch_block_header(block->hash(), handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block_header2__exists__success)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE_EQUAL(fetch_block_header_by_hash_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block_header2__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_block_header_by_hash_result(instance, block1, 1), error::not_found);
}

// fetch_merkle_block

static int fetch_merkle_block_by_height_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, merkle_block_ptr result_merkle,
        size_t result_height)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        const auto match = result_height == height &&
            *result_merkle == message::merkle_block(*block);
        promise.set_value(match ? error::success : error::operation_failed);
    };
    instance.fetch_merkle_block(height, handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block1__exists__success)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE_EQUAL(fetch_merkle_block_by_height_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block1__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_merkle_block_by_height_result(instance, block1, 1), error::not_found);
}

static int fetch_merkle_block_by_hash_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, merkle_block_ptr result_merkle,
        size_t result_height)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        const auto match = result_height == height &&
            *result_merkle == message::merkle_block(*block);
        promise.set_value(match ? error::success : error::operation_failed);
    };
    instance.fetch_merkle_block(block->hash(), handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block2__exists__success)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE_EQUAL(fetch_merkle_block_by_hash_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block2__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_merkle_block_by_hash_result(instance, block1, 1), error::not_found);
}

// TODO: fetch_block_height
// TODO: fetch_last_height
// TODO: fetch_transaction
// TODO: fetch_transaction_position
// TODO: fetch_output
// TODO: fetch_spend
// TODO: fetch_history
// TODO: fetch_stealth
// TODO: fetch_block_locator
// TODO: fetch_locator_block_hashes

// fetch_locator_block_headers

static int fetch_locator_block_headers(block_chain& instance,
    get_headers_const_ptr locator, const hash_digest& threshold, size_t limit)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, headers_ptr result_headers)
    {
        if (ec)
        {
            promise.set_value(ec);
            return;
        }

        // TODO: incorporate other expectations.
        const auto sequential = result_headers->is_sequential();

        promise.set_value(sequential ? error::success : error::operation_failed);
    };
    instance.fetch_locator_block_headers(locator, threshold, limit, handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_locator_block_headers__empty__sequential)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    const auto block3 = NEW_BLOCK(3);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));
    BOOST_REQUIRE(instance.push(block3, 3, 0));

    const auto locator = std::make_shared<const message::get_headers>();
    BOOST_REQUIRE_EQUAL(fetch_locator_block_headers(instance, locator, null_hash, 0), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_locator_block_headers__full__sequential)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    const auto block3 = NEW_BLOCK(3);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));
    BOOST_REQUIRE(instance.push(block3, 3, 0));

    const size_t limit = 3;
    const auto threshold = null_hash;
    const auto locator = std::make_shared<const message::get_headers>();
    BOOST_REQUIRE_EQUAL(fetch_locator_block_headers(instance, locator, null_hash, 3), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_locator_block_headers__limited__sequential)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    const auto block3 = NEW_BLOCK(3);
    BOOST_REQUIRE(instance.push(block1, 1, 0));
    BOOST_REQUIRE(instance.push(block2, 2, 0));
    BOOST_REQUIRE(instance.push(block3, 3, 0));

    const size_t limit = 3;
    const auto threshold = null_hash;
    const auto locator = std::make_shared<const message::get_headers>();
    BOOST_REQUIRE_EQUAL(fetch_locator_block_headers(instance, locator, null_hash, 2), error::success);
}

// TODO: fetch_template
// TODO: fetch_mempool
// TODO: filter_blocks
// TODO: filter_transactions
// TODO: subscribe_blockchain
// TODO: subscribe_transaction
// TODO: unsubscribe
// TODO: organize_block
// TODO: organize_transaction
// TODO: chain_settings
// TODO: stopped
// TODO: to_hashes

BOOST_AUTO_TEST_SUITE_END()
