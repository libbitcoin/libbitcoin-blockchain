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

#include <future>
#include <string>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;
using namespace boost::system;
using namespace boost::filesystem;

#define MAINNET_BLOCK1 \
"010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000982" \
"051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e61bc6649ffff00" \
"1d01e3629901010000000100000000000000000000000000000000000000000000000000000" \
"00000000000ffffffff0704ffff001d0104ffffffff0100f2052a0100000043410496b538e8" \
"53519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a60" \
"4f8141781e62294721166bf621e73a82cbf2342c858eeac00000000"

#define MAINNET_BLOCK2 \
"010000004860eb18bf1b1620e37e9490fc8a427514416fd75159ab86688e9a8300000000d5f" \
"dcc541e25de1c7a5addedf24858b8bb665c9f36ef744ee42c316022c90f9bb0bc6649ffff00" \
"1d08d2bd6101010000000100000000000000000000000000000000000000000000000000000" \
"00000000000ffffffff0704ffff001d010bffffffff0100f2052a010000004341047211a824" \
"f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073dee6c89064984f03385" \
"237d92167c13e236446b417ab79a0fcae412ae3316b77ac00000000"

#define MAINNET_BLOCK3 \
"01000000bddd99ccfda39da1b108ce1a5d70038d0a967bacb68b6b63065f626a0000000044f" \
"672226090d85db9a9f2fbfe5f0f9609b387af7be5b7fbb7a1767c831c9e995dbe6649ffff00" \
"1d05e0ed6d01010000000100000000000000000000000000000000000000000000000000000" \
"00000000000ffffffff0704ffff001d010effffffff0100f2052a0100000043410494b9d3e7" \
"6c5b1629ecf97fff95d7a4bbdac87cc26099ada28066c6ff1eb9191223cd897194a08d0c272" \
"6c5747f1db49e8cf90e75dc3e3550ae9b30086f3cd5aaac00000000"

#define TEST_SET_NAME \
    "p2p_tests"

#define TEST_NAME \
    std::string(boost::unit_test::framework::current_test_case().p_name)

#define OPEN_BLOCKCHAIN(name, flush) \
    threadpool pool; \
    database::settings database_settings; \
    database_settings.directory = TEST_NAME; \
    BOOST_REQUIRE(create_database(database_settings)); \
    blockchain::settings blockchain_settings; \
    blockchain_settings.flush_writes = flush; \
    block_chain name(pool, blockchain_settings, database_settings); \
    BOOST_REQUIRE(name.open())

#define START_BLOCKCHAIN(name, flush) \
    OPEN_BLOCKCHAIN(name, flush); \
    BOOST_REQUIRE(name.start())

#define NEW_BLOCK(height) \
    std::make_shared<const message::block>(read_block(MAINNET_BLOCK##height))

static const uint64_t genesis_mainnet_work = 0x0000000100010001;

static void print_headers(const std::string& test)
{
    const auto header = "=========== " + test + " ==========";
    LOG_INFO(TEST_SET_NAME) << header;
}

bool create_database(database::settings& out_database)
{
    print_headers(out_database.directory.string());

    // Blockchain doesn't care about other indexes.
    out_database.index_start_height = max_uint32;

    // Table optimization parameters, reduced for speed and more collision.
    out_database.file_growth_rate = 42;
    out_database.block_table_buckets = 42;
    out_database.transaction_table_buckets = 42;
    out_database.spend_table_buckets = 42;
    out_database.history_table_buckets = 42;

    error_code ec;
    remove_all(out_database.directory, ec);
    database::data_base database(out_database);
    return create_directories(out_database.directory, ec) &&
        database.create(chain::block::genesis_mainnet());
}

chain::block read_block(const std::string hex)
{
    data_chunk data;
    BOOST_REQUIRE(decode_base16(data, hex));
    chain::block result;
    BOOST_REQUIRE(result.from_data(data));
    return result;
}

BOOST_AUTO_TEST_SUITE(fast_chain_tests)

BOOST_AUTO_TEST_CASE(block_chain__insert__flushed__expected)
{
    OPEN_BLOCKCHAIN(instance, true);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.get_block_exists(block1->hash()));
    BOOST_REQUIRE(instance.get_block_exists(chain::block::genesis_mainnet().hash()));
}

BOOST_AUTO_TEST_CASE(block_chain__insert__unflushed__expected_block)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.get_block_exists(block1->hash()));
    BOOST_REQUIRE(instance.get_block_exists(chain::block::genesis_mainnet().hash()));
}

BOOST_AUTO_TEST_CASE(block_chain__get_gaps__none__none)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    database::block_database::heights heights;
    BOOST_REQUIRE(instance.get_gaps(heights));
    BOOST_REQUIRE(heights.empty());
}

BOOST_AUTO_TEST_CASE(block_chain__get_gaps__one__one)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block2, 2));

    database::block_database::heights heights;
    BOOST_REQUIRE(instance.get_gaps(heights));
    BOOST_REQUIRE_EQUAL(heights.size(), 1u);
    BOOST_REQUIRE_EQUAL(heights[0], 1u);
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_hash__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    hash_digest hash;
    BOOST_REQUIRE(!instance.get_block_hash(hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_hash__found__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    hash_digest hash;
    BOOST_REQUIRE(instance.get_block_hash(hash, 1));
    BOOST_REQUIRE(hash == block1->hash());
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__height_above_top__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    uint256_t work;
    uint256_t maximum(max_uint64);

    // This is allowed and just returns zero (standard new single block).
    BOOST_REQUIRE(instance.get_branch_work(work, maximum, 1));
    BOOST_REQUIRE_EQUAL(work, 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__maximum_zero__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    uint256_t work;
    uint256_t maximum(0);

    // This should exit early due to hitting the maximum before the genesis block.
    BOOST_REQUIRE(instance.get_branch_work(work, maximum, 0));
    BOOST_REQUIRE_EQUAL(work, 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__maximum_one__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
    uint256_t work;
    uint256_t maximum(genesis_mainnet_work);

    // This should exit early due to hitting the maximum on the genesis block.
    BOOST_REQUIRE(instance.get_branch_work(work, maximum, 0));
    BOOST_REQUIRE_EQUAL(work, genesis_mainnet_work);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__unbounded__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    uint256_t work;
    uint256_t maximum(max_uint64);

    // This should not exit early.
    BOOST_REQUIRE(instance.get_branch_work(work, maximum, 0));
    BOOST_REQUIRE_EQUAL(work, 0x0000000300030003);
}

BOOST_AUTO_TEST_CASE(block_chain__get_header__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    chain::header header;
    BOOST_REQUIRE(!instance.get_header(header, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_header__found__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    chain::header header;
    BOOST_REQUIRE(instance.get_header(header, 1));
    BOOST_REQUIRE(header == block1->header());
}

BOOST_AUTO_TEST_CASE(block_chain__get_height__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    size_t height;
    BOOST_REQUIRE(!instance.get_height(height, null_hash));
}

BOOST_AUTO_TEST_CASE(block_chain__get_height__found__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    size_t height;
    BOOST_REQUIRE(instance.get_height(height, block1->hash()));
    BOOST_REQUIRE_EQUAL(height, 1u);
}

BOOST_AUTO_TEST_CASE(block_chain__get_bits__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    uint32_t bits;
    BOOST_REQUIRE(!instance.get_bits(bits, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_bits__found__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    uint32_t bits;
    BOOST_REQUIRE(instance.get_bits(bits, 1));
    BOOST_REQUIRE_EQUAL(bits, block1->header().bits());
}

BOOST_AUTO_TEST_CASE(block_chain__get_timestamp__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    uint32_t timestamp;
    BOOST_REQUIRE(!instance.get_timestamp(timestamp, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_timestamp__found__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    uint32_t timestamp;
    BOOST_REQUIRE(instance.get_timestamp(timestamp, 1));
    BOOST_REQUIRE_EQUAL(timestamp, block1->header().timestamp());
}

BOOST_AUTO_TEST_CASE(block_chain__get_version__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    uint32_t version;
    BOOST_REQUIRE(!instance.get_version(version, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_version__found__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));

    uint32_t version;
    BOOST_REQUIRE(instance.get_version(version, 1));
    BOOST_REQUIRE_EQUAL(version, block1->header().version());
}

BOOST_AUTO_TEST_CASE(block_chain__get_last_height__no_gaps__last_block)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    size_t height;
    BOOST_REQUIRE(instance.get_last_height(height));
    BOOST_REQUIRE_EQUAL(height, 2u);
}

BOOST_AUTO_TEST_CASE(block_chain__get_last_height__gap__last_block)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block2, 2));

    size_t height;
    BOOST_REQUIRE(instance.get_last_height(height));
    BOOST_REQUIRE_EQUAL(height, 2u);
}

BOOST_AUTO_TEST_CASE(block_chain__get_output__not_found__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    chain::output output;
    size_t height;
    bool coinbase;
    const chain::output_point outpoint{ null_hash, 42 };
    size_t branch_height = 0;
    BOOST_REQUIRE(!instance.get_output(output, height, coinbase, outpoint, branch_height));
}

BOOST_AUTO_TEST_CASE(block_chain__get_output__found__expected)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    chain::output output;
    size_t height;
    bool coinbase;
    const chain::output_point outpoint{ block2->transactions()[0].hash(), 0 };
    const auto expected_value = initial_block_reward_satoshi();
    const auto expected_script = block2->transactions()[0].outputs()[0].script().to_string(0);
    BOOST_REQUIRE(instance.get_output(output, height, coinbase, outpoint, 2));
    BOOST_REQUIRE(coinbase);
    BOOST_REQUIRE_EQUAL(height, 2u);
    BOOST_REQUIRE_EQUAL(output.value(), expected_value);
    BOOST_REQUIRE_EQUAL(output.script().to_string(0), expected_script);
}

BOOST_AUTO_TEST_CASE(block_chain__get_output__above_fork__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    chain::output output;
    size_t height;
    bool coinbase;
    const chain::output_point outpoint{ block2->transactions()[0].hash(), 0 };
    BOOST_REQUIRE(!instance.get_output(output, height, coinbase, outpoint, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_is_unspent_transaction__unspent_at_fork__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    const auto hash = block2->transactions()[0].hash();
    BOOST_REQUIRE(instance.get_is_unspent_transaction(hash, 2));
}

BOOST_AUTO_TEST_CASE(block_chain__get_is_unspent_transaction__unspent_above_fork__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    const auto hash = block2->transactions()[0].hash();
    BOOST_REQUIRE(!instance.get_is_unspent_transaction(hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_is_unspent_transaction__spent_below_fork__false)
{
    // TODO: generate spent tx test vector.
}

BOOST_AUTO_TEST_CASE(block_chain__get_transaction__exists__true)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    size_t height;
    const auto hash = block1->transactions()[0].hash();
    BOOST_REQUIRE(instance.get_transaction(height, hash));
    BOOST_REQUIRE_EQUAL(height, 1u);
}

BOOST_AUTO_TEST_CASE(block_chain__get_transaction__not_exists_and_gapped__false)
{
    OPEN_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block2, 2));

    size_t height;
    const auto hash = block1->transactions()[0].hash();
    BOOST_REQUIRE(!instance.get_transaction(height, hash));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(safe_chain_tests)

// fetch_block

static int fetch_block_by_height_result(block_chain& instance,
    block_const_ptr block, size_t height)
{
    std::promise<code> promise;
    const auto handler = [=, &promise](code ec, block_ptr result_block,
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
    instance.fetch_block(height, handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block1__unstarted__error_service_stopped)
{
    threadpool pool;
    database::settings database_settings;
    database_settings.directory = TEST_NAME;
    BOOST_REQUIRE(create_database(database_settings));

    blockchain::settings blockchain_settings;
    block_chain instance(pool, blockchain_settings, database_settings);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_block_by_height_result(instance, block1, 1), error::service_stopped);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block1__exists__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
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
    const auto handler = [=, &promise](code ec, block_ptr result_block,
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
    instance.fetch_block(block->hash(), handler);
    return promise.get_future().get().value();
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_block2__exists__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
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

BOOST_AUTO_TEST_CASE(block_chain__fetch_block_header1__exists__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
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

BOOST_AUTO_TEST_CASE(block_chain__fetch_block_header2__exists__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
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

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block1__exists__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
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

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block2__exists__true)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE_EQUAL(fetch_merkle_block_by_hash_result(instance, block1, 1), error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__fetch_merkle_block2__not_exists__error_not_found)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    BOOST_REQUIRE_EQUAL(fetch_merkle_block_by_hash_result(instance, block1, 1), error::not_found);
}

BOOST_AUTO_TEST_SUITE_END()
