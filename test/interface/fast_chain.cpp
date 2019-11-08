/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/blockchain.hpp>
#include "../utility.hpp"

using namespace bc;
using namespace bc::system;
using namespace bc::blockchain;
using namespace bc::database;

#define OUTPUT_SCRIPT0 "dup hash160 [58350574280395ad2c3e2ee20e322073d94e5e40] equalverify checksig"

#define TEST_SET_NAME                           \
   "fast_chain_tests"

class block_chain_accessor
  : public block_chain
{
public:
    transaction_const_ptr notified_with;
    transaction_const_ptr cataloged_with;

    block_chain_accessor(threadpool& pool, const blockchain::settings& settings,
        const database::settings& database_settings,
        const system::settings& bitcoin_settings)
      : block_chain(pool, settings, database_settings, bitcoin_settings)
    {
    }

    bool start()
    {
        return block_chain::start();
    }

    database::data_base& database()
    {
        return database_;
    }

    transaction_const_ptr last_pool_transaction() const
    {
        return last_pool_transaction_.load();
    }

    void notify(transaction_const_ptr tx)
    {
        notified_with = tx;
    }

    void catalog_transaction(system::transaction_const_ptr tx)
    {
        cataloged_with = tx;
    }

};

class fast_chain_setup_fixture
{
public:
    fast_chain_setup_fixture()
    {
        test::remove_test_directory(TEST_NAME);
    }

    ~fast_chain_setup_fixture()
    {
        test::remove_test_directory(TEST_NAME);
    }
};

BOOST_FIXTURE_TEST_SUITE(fast_chain_tests, fast_chain_setup_fixture)

BOOST_AUTO_TEST_CASE(block_chain__getters__candidate_and_confirmed__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();

    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    // Setup ends.

    // Test conditions.
    config::checkpoint top;
    chain::header out_header;
    size_t out_height;
    BOOST_REQUIRE(instance.get_top(top, true));
    BOOST_REQUIRE_EQUAL(top.height(), 2u);
    BOOST_REQUIRE(top.hash() == block2->hash());

    BOOST_REQUIRE(instance.get_top(out_header, out_height, true));
    BOOST_REQUIRE_EQUAL(out_height, 2u);
    BOOST_REQUIRE(out_header.hash() == block2->hash());

    BOOST_REQUIRE(instance.get_top(top, false));
    BOOST_REQUIRE_EQUAL(top.height(), 0u);
    BOOST_REQUIRE(top.hash() == genesis.hash());

    BOOST_REQUIRE(instance.get_top(out_header, out_height, false));
    BOOST_REQUIRE_EQUAL(out_height, 0u);
    BOOST_REQUIRE(out_header.hash() == genesis.hash());

    // Confirm blocks
    database.invalidate(block1->header(), error::success);
    database.update(*block1, 1);
    database.invalidate(block2->header(), error::success);
    database.update(*block2, 2);
    const auto incoming_blocks = std::make_shared<const block_const_ptr_list>(block_const_ptr_list{ block1, block2 });
    const auto outgoing_blocks = std::make_shared<block_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_blocks, outgoing_blocks), error::success);

    // Test conditions.
    BOOST_REQUIRE(instance.get_top(top, true));
    BOOST_REQUIRE_EQUAL(top.height(), 2u);
    BOOST_REQUIRE(top.hash() == block2->hash());

    BOOST_REQUIRE(instance.get_top(out_header, out_height, true));
    BOOST_REQUIRE_EQUAL(out_height, 2u);
    BOOST_REQUIRE(out_header.hash() == block2->hash());

    BOOST_REQUIRE(instance.get_top(top, false));
    BOOST_REQUIRE_EQUAL(top.height(), 2u);
    BOOST_REQUIRE(top.hash() == block2->hash());

    BOOST_REQUIRE(instance.get_top(out_header, out_height, false));
    BOOST_REQUIRE_EQUAL(out_height, 2u);
    BOOST_REQUIRE(out_header.hash() == block2->hash());
}

BOOST_AUTO_TEST_CASE(block_chain__get_header2___present_and_not__true_and_false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1->header(), error::success);
    database.update(*block1, 1);
    const auto incoming_blocks = std::make_shared<const block_const_ptr_list>(block_const_ptr_list{ block1 });
    const auto outgoing_blocks = std::make_shared<block_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_blocks, outgoing_blocks), error::success);

    // Setup ends.

    // Test conditions.
    chain::header out_header;
    size_t out_height;
    BOOST_REQUIRE(!instance.get_header(out_header, out_height, block2->hash(), true));
    BOOST_REQUIRE(!instance.get_header(out_header, out_height, block2->hash(), false));

    BOOST_REQUIRE(instance.get_header(out_header, out_height, block1->hash(), true));
    BOOST_REQUIRE_EQUAL(out_height, 1u);
    BOOST_REQUIRE(out_header == block1->header());

    BOOST_REQUIRE(instance.get_header(out_header, out_height, block1->hash(), false));
    BOOST_REQUIRE_EQUAL(out_height, 1u);
    BOOST_REQUIRE(out_header == block1->header());
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_error___present_and_not__true_and_false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1->header(), error::success);
    database.update(*block1, 1);
    const auto incoming_blocks = std::make_shared<const block_const_ptr_list>(block_const_ptr_list{ block1 });
    const auto outgoing_blocks = std::make_shared<block_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_blocks, outgoing_blocks), error::success);

    // Setup ends.

    // Test conditions.
    code out_error;
    BOOST_REQUIRE(!instance.get_block_error(out_error, block2->hash()));

    BOOST_REQUIRE(instance.get_block_error(out_error, block1->hash()));
    BOOST_REQUIRE_EQUAL(out_error, error::success);
}

BOOST_AUTO_TEST_CASE(block_chain__get_bits__not_found__false)
{
    START_BLOCKCHAIN(instance, false, true);

   uint32_t out_bits;
   BOOST_REQUIRE(!instance.get_bits(out_bits, 1, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_bits___present_and_not__true_and_false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1->header(), error::success);
    database.update(*block1, 1);
    const auto incoming_blocks = std::make_shared<const block_const_ptr_list>(block_const_ptr_list{ block1 });
    const auto outgoing_blocks = std::make_shared<block_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_blocks, outgoing_blocks), error::success);

    // Setup ends.

    // Test conditions.
    uint32_t out_bits;
    BOOST_REQUIRE(instance.get_bits(out_bits, 2, true));
    BOOST_REQUIRE_EQUAL(out_bits, block2->header().bits());

    BOOST_REQUIRE(instance.get_bits(out_bits, 1, false));
    BOOST_REQUIRE_EQUAL(out_bits, block1->header().bits());

    BOOST_REQUIRE(!instance.get_bits(out_bits, 2, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_timestamp__not_found__false)
{
    START_BLOCKCHAIN(instance, false, true);

   uint32_t timestamp;
   BOOST_REQUIRE(!instance.get_timestamp(timestamp, 1, true));
   BOOST_REQUIRE(!instance.get_timestamp(timestamp, 1, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_timestamp__found__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    uint32_t timestamp;
    BOOST_REQUIRE(instance.get_timestamp(timestamp, 1, true));
    BOOST_REQUIRE_EQUAL(timestamp, block1->header().timestamp());

    BOOST_REQUIRE(!instance.get_timestamp(timestamp, 1, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_version__not_found__false)
{
    START_BLOCKCHAIN(instance, false, true);

   uint32_t version;
   BOOST_REQUIRE(!instance.get_version(version, 1, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_version__found__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    uint32_t version;
    BOOST_REQUIRE(instance.get_version(version, 1, true));
    BOOST_REQUIRE_EQUAL(version, block1->header().version());

    BOOST_REQUIRE(!instance.get_version(version, 1, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_version___present_and_not__true_and_false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;

    auto& database = instance.database();

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);

    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1->header(), error::success);
    database.update(*block1, 1);
    const auto incoming_blocks = std::make_shared<const block_const_ptr_list>(block_const_ptr_list{ block1 });
    const auto outgoing_blocks = std::make_shared<block_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_blocks, outgoing_blocks), error::success);

    // Setup ends.

    // Test conditions.
    uint32_t out_version;
    BOOST_REQUIRE(instance.get_version(out_version, 2, true));
    BOOST_REQUIRE_EQUAL(out_version, block2->header().version());

    BOOST_REQUIRE(instance.get_version(out_version, 1, false));
    BOOST_REQUIRE_EQUAL(out_version, block1->header().version());

    BOOST_REQUIRE(!instance.get_version(out_version, 2, false));
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__genesis_confirmed__zero)
{
    START_BLOCKCHAIN(instance, false, true);

    uint256_t work;
    uint256_t overcome(max_uint64);

    // This is allowed and just returns zero (standard new single block).
    BOOST_REQUIRE(instance.get_work(work, overcome, 0, false));
    BOOST_REQUIRE_EQUAL(work, 0);

    BOOST_REQUIRE(instance.get_work(work, overcome, 0, true));
    BOOST_REQUIRE_EQUAL(work, 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get__work__height_above_top__true)
{
    START_BLOCKCHAIN(instance, false, true);

    uint256_t work;
    uint256_t overcome(max_uint64);

    // This is allowed and just returns zero (standard new single block).
    BOOST_REQUIRE(instance.get_work(work, overcome, 1, false));
    BOOST_REQUIRE_EQUAL(work, 0);
}

// NOTE: Old test, doesn't work now. Should be deleted?
// BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__overcome_zero__true)
// {
//     const uint64_t genesis_mainnet_work = 0x0000000100010001;
//     START_BLOCKCHAIN(instance, false);

//     uint256_t work;
//     uint256_t overcome(0);

//     // This should not exit early.
//     BOOST_REQUIRE(instance.get_work(work, overcome, 0, false));
//     BOOST_REQUIRE_EQUAL(work, genesis_mainnet_work);
// }

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__maximum_one__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    uint256_t work;
    uint256_t overcome(block1->header().proof());

    // This should not exit early due to tying on the first block (order matters).
    BOOST_REQUIRE(instance.get_work(work, overcome, 0, true));
    BOOST_REQUIRE_EQUAL(work, block1->header().proof());

    BOOST_REQUIRE(instance.get_work(work, overcome, 0, false));
    BOOST_REQUIRE_EQUAL(work, 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__unbounded__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    uint256_t work;
    uint256_t overcome(max_uint64);

    // This should not exit early but skips the genesis block.
    BOOST_REQUIRE(instance.get_work(work, overcome, 0, true));
    BOOST_REQUIRE_EQUAL(work, block1->header().proof() + block2->header().proof());

    BOOST_REQUIRE(instance.get_work(work, overcome, 0, false));
    BOOST_REQUIRE_EQUAL(work, 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_branch_work__confirmed_unbounded__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1->header()),
        std::make_shared<const message::header>(block2->header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1->header(), error::success);
    database.update(*block1, 1);
    database.invalidate(block2->header(), error::success);
    database.update(*block2, 2);
    const auto incoming_blocks = std::make_shared<const block_const_ptr_list>(block_const_ptr_list{ block1, block2 });
    const auto outgoing_blocks = std::make_shared<block_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_blocks, outgoing_blocks), error::success);

    // Setup ends.

    uint256_t work;
    uint256_t overcome(max_uint64);

    // This should not exit early but skips the genesis block.
    BOOST_REQUIRE(instance.get_work(work, overcome, 0, true));
    BOOST_REQUIRE_EQUAL(work, block1->header().proof() + block2->header().proof());

    BOOST_REQUIRE(instance.get_work(work, overcome, 0, false));
    BOOST_REQUIRE_EQUAL(work, block1->header().proof() + block2->header().proof());
}

BOOST_AUTO_TEST_CASE(block_chain__get_downloadable__not_present__false)
{
    START_BLOCKCHAIN(instance, false, true);

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_downloadable(out_hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_downloadable__present_failed_state__false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = test::read_block(MAINNET_BLOCK1);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1.header(), error::insufficient_fee);

    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_downloadable(out_hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_downloadable__present_with_transactions__false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = test::read_block(MAINNET_BLOCK1);
    const auto block2 = test::read_block(MAINNET_BLOCK2);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    BOOST_REQUIRE_EQUAL(database.blocks().get(0, true).transaction_count(), 1);
    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_downloadable(out_hash, 0));
}

BOOST_AUTO_TEST_CASE(block_chain__get_downloadable__present_with_transactions__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 0);
    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(instance.get_downloadable(out_hash, 1));
    BOOST_REQUIRE(out_hash == block1.hash());
}

// get_validatable

BOOST_AUTO_TEST_CASE(block_chain__get_validatable__not_present__false)
{
    START_BLOCKCHAIN(instance, false, true);

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_validatable(out_hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_validatable__present_and_failed_state__false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = test::read_block(MAINNET_BLOCK1);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1.header(), error::insufficient_fee);

    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_validatable(out_hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_validatable__present_and_valid__false)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    const auto block1 = test::read_block(MAINNET_BLOCK1);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    database.invalidate(block1.header(), error::success);

    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_validatable(out_hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_validatable__present_without_transactions__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);

    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 0);

    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(!instance.get_validatable(out_hash, 1));
}

BOOST_AUTO_TEST_CASE(block_chain__get_validatable__present_with_transactions__true)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    hash_digest out_hash;
    BOOST_REQUIRE(instance.get_validatable(out_hash, 1));
    BOOST_REQUIRE(out_hash == block1.hash());
}

BOOST_AUTO_TEST_CASE(block_chain__populate_header__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    auto& header1 = block1.header();
    BOOST_REQUIRE(!header1.metadata.candidate);

    // Setup ends.

    instance.populate_header(header1);
    BOOST_REQUIRE(header1.metadata.candidate);
}

BOOST_AUTO_TEST_CASE(block_chain__populate_block_transaction__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });

    auto transaction1 = block1.transactions().front();
    BOOST_REQUIRE(!transaction1.metadata.candidate);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    instance.populate_block_transaction(transaction1, 1, 1);
    BOOST_REQUIRE(!transaction1.metadata.candidate);
}

BOOST_AUTO_TEST_CASE(block_chain__populate_pool_transaction__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });

    auto transaction1 = block1.transactions().front();
    BOOST_REQUIRE(!transaction1.metadata.candidate);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    instance.populate_pool_transaction(transaction1, 1);
    BOOST_REQUIRE(!transaction1.metadata.candidate);
}

BOOST_AUTO_TEST_CASE(block_chain__populate_block_output__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });

    auto transaction1 = block1.transactions().front();
    BOOST_REQUIRE(!transaction1.metadata.candidate);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    const auto& prevout = transaction1.inputs()[0].previous_output();
    instance.populate_block_output(prevout, 1);
    BOOST_REQUIRE(!prevout.metadata.candidate);
    BOOST_REQUIRE(!prevout.metadata.confirmed);
}

BOOST_AUTO_TEST_CASE(block_chain__populate_pool_output__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });

    auto transaction1 = block1.transactions().front();
    BOOST_REQUIRE(!transaction1.metadata.candidate);
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    const auto& prevout = transaction1.inputs()[0].previous_output();
    instance.populate_pool_output(prevout);
    BOOST_REQUIRE(!prevout.metadata.candidate);
    BOOST_REQUIRE(!prevout.metadata.confirmed);
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_state__not_present__missing)
{
    START_BLOCKCHAIN(instance, false, true);

    // Setup ends.

    uint8_t state = instance.get_block_state(10, true);
    BOOST_REQUIRE(state == 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_state__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    uint8_t state = instance.get_block_state(1, true);
    BOOST_REQUIRE((state & block_state::candidate) != 0);
    BOOST_REQUIRE((state & block_state::confirmed) == 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_state2__not_present__missing)
{
    START_BLOCKCHAIN(instance, false, true);

    // Setup ends.

    uint8_t state = instance.get_block_state(hash_digest{});
    BOOST_REQUIRE(state == 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_block_state2__present__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    uint8_t state = instance.get_block_state(block1.hash());
    BOOST_REQUIRE((state & block_state::candidate) != 0);
    BOOST_REQUIRE((state & block_state::confirmed) == 0);
}

BOOST_AUTO_TEST_CASE(block_chain__get_candidate__not_present__missing)
{
    START_BLOCKCHAIN(instance, false, true);

    // Setup ends.

    const auto block = instance.get_candidate(10);
    BOOST_REQUIRE(block == nullptr);
}

BOOST_AUTO_TEST_CASE(block_chain__get_candidate__present_without_transactions__missing)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({});
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 0);

    // Setup ends.

    const auto block = instance.get_candidate(1);
    BOOST_REQUIRE(block == nullptr);
}

BOOST_AUTO_TEST_CASE(block_chain__get_candidate__present_with_transactions__success)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    auto& database = instance.database();
    auto block1 = test::read_block(MAINNET_BLOCK1);
    block1.set_transactions({ test::random_tx(0), test::random_tx(1) });
    const auto incoming_headers = std::make_shared<const header_const_ptr_list>(header_const_ptr_list
    {
        std::make_shared<const message::header>(block1.header()),
    });
    const auto outgoing_headers = std::make_shared<header_const_ptr_list>();
    BOOST_REQUIRE_EQUAL(database.reorganize({genesis.hash(), 0}, incoming_headers, outgoing_headers), error::success);
    BOOST_REQUIRE(bc::database::is_candidate(database.blocks().get(1, true).state()));
    database.update(block1, 1);
    BOOST_REQUIRE_EQUAL(database.blocks().get(1, true).transaction_count(), 2);

    // Setup ends.

    const auto block = instance.get_candidate(1);
    BOOST_REQUIRE(block->header().previous_block_hash() == genesis.hash());
    BOOST_REQUIRE(block->header().merkle_root() != null_hash);
    BOOST_REQUIRE(block->transactions().size() == 2);
}

BOOST_AUTO_TEST_CASE(block_chain__get_header__not_present__missing)
{
    START_BLOCKCHAIN(instance, false, true);

    // Setup ends.

    const auto block = instance.get_header(10, true);
    BOOST_REQUIRE(block == nullptr);
}

// Writers.

BOOST_AUTO_TEST_CASE(block_chain__store__no_state__failure)
{
    START_BLOCKCHAIN(instance, false, true);
    auto& database = instance.database();
    auto transaction = std::make_shared<const message::transaction>(test::random_tx(0));
    BOOST_REQUIRE(!transaction->metadata.state);

    // Setup ends.

    BOOST_REQUIRE_EQUAL(instance.store(transaction), error::operation_failed);
    BOOST_REQUIRE(!database.transactions().get(transaction->hash()));
}

BOOST_AUTO_TEST_CASE(block_chain__store__duplicate_transaction__failure)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    BOOST_REQUIRE_EQUAL(genesis.transactions().size(), 1);
    auto transaction = std::make_shared<const message::transaction>(genesis.transactions()[0]);
    transaction->metadata.state = instance.next_confirmed_state();

    // Setup ends.

#ifndef NDEBUG
    BOOST_REQUIRE_EQUAL(instance.store(transaction), error::duplicate_transaction);
#else
    BOOST_REQUIRE(!instance.store(transaction));
#endif
}

BOOST_AUTO_TEST_CASE(block_chain__store__without_cataloging__success)
{
    START_BLOCKCHAIN(instance, false, false);
    auto& database = instance.database();
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    BOOST_REQUIRE_EQUAL(genesis.transactions().size(), 1);
    auto transaction = std::make_shared<const message::transaction>(test::random_tx(0));
    const auto initial_state = instance.next_confirmed_state();
    transaction->metadata.state = initial_state;

    // Setup ends.

    BOOST_REQUIRE_EQUAL(instance.store(transaction), error::success);

    // Transaction is present in database.
    const auto reloaded = database.transactions().get(transaction->hash());
    BOOST_REQUIRE(reloaded);

    const auto reloaded_transaction = reloaded.transaction();

    // Transaction metadata says existed.
    BOOST_REQUIRE(reloaded_transaction.metadata.existed);

    // Last pool transaction is updated.
    BOOST_REQUIRE(instance.last_pool_transaction() == transaction);

    // invoke() called on transaction subscriber.
    BOOST_REQUIRE(instance.notified_with == transaction);

    // Transaction is not cataloged.
    BOOST_REQUIRE(instance.cataloged_with == nullptr);
}

BOOST_AUTO_TEST_CASE(block_chain__store__with_cataloging_tx_metadata_existed__failure)
{
    START_BLOCKCHAIN(instance, false, true);
    const auto bc_settings = bc::system::settings(config::settings::mainnet);
    const chain::block& genesis = bc_settings.genesis_block;
    BOOST_REQUIRE_EQUAL(genesis.transactions().size(), 1);


    uint32_t version = 2345u;
    uint32_t locktime = 0xffffffff;

    chain::script script0;
    script0.from_string(OUTPUT_SCRIPT0);

    const chain::input::list inputs
    {
        { chain::point{ null_hash, chain::point::null_index }, {}, 0 }
    };

    const chain::output::list outputs
    {
        { 1200, script0 }
    };

    chain::transaction tx{ version, locktime, inputs, outputs };

    auto transaction = std::make_shared<const message::transaction>(tx);
    const auto initial_state = instance.next_confirmed_state();
    transaction->metadata.state = initial_state;

    // Setup ends.

    BOOST_REQUIRE_EQUAL(instance.store(transaction), error::success);

    // Transaction is cataloged.
    BOOST_REQUIRE(instance.cataloged_with == transaction);
}

BOOST_AUTO_TEST_CASE(block_chain__store__with_cataloging_tx_metadata_not_existed__success)
{
    // Transaction is present in database.
    // Transaction metadata state is updated.
    // Last pool transaction is updated.
    // Transaction subscriber is invoked.
    // Transaction is cataloged.
}

////BOOST_AUTO_TEST_CASE(block_chain__push__flushed__expected)
////{
////    START_BLOCKCHAIN(instance, true);
////
////    const auto block1 = NEW_BLOCK(1);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////    const auto state1 = instance.get_block_state(block1->hash());
////    BOOST_REQUIRE(is_confirmed(state1));
////    const auto state0 = instance.get_block_state(chain::block::genesis_mainnet().hash());
////    BOOST_REQUIRE(is_confirmed(state0));
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__push__unflushed__expected_block)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const auto block1 = NEW_BLOCK(1);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////    const auto state1 = instance.get_block_state(block1->hash());
////    BOOST_REQUIRE(is_confirmed(state1));
////    const auto state0 = instance.get_block_state(chain::block::genesis_mainnet().hash());
////    BOOST_REQUIRE(is_confirmed(state0));
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__get_block_hash__not_found__false)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    hash_digest hash;
////    BOOST_REQUIRE(!instance.get_block_hash(hash, 1, false));
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__get_block_hash__found__true)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const auto block1 = NEW_BLOCK(1);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////
////    hash_digest hash;
////    BOOST_REQUIRE(instance.get_block_hash(hash, 1, false));
////    BOOST_REQUIRE(hash == block1->hash());
////}
////
////
////////BOOST_AUTO_TEST_CASE(block_chain__get_height__not_found__false)
////////{
////////    START_BLOCKCHAIN(instance, false);
////////
////////    size_t height;
////////    BOOST_REQUIRE(!instance.get_block_height(height, null_hash, true));
////////}
////////
////////BOOST_AUTO_TEST_CASE(block_chain__get_height__found__true)
////////{
////////    START_BLOCKCHAIN(instance, false);
////////
////////    const auto block1 = NEW_BLOCK(1);
////////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////////
////////    size_t height;
////////    BOOST_REQUIRE(instance.get_block_height(height, block1->hash(), true));
////////    BOOST_REQUIRE_EQUAL(height, 1u);
////////}
////
////
////BOOST_AUTO_TEST_CASE(block_chain__populate_output__not_found__false)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const chain::output_point outpoint{ null_hash, 42 };
////    size_t header_branch_height = 0;
////    instance.populate_output(outpoint, header_branch_height);
////    BOOST_REQUIRE(!outpoint.metadata.cache.is_valid());
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__populate_output__found__expected)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const auto block1 = NEW_BLOCK(1);
////    const auto block2 = NEW_BLOCK(2);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////    BOOST_REQUIRE(instance.push(block2, 2, 0));
////
////    const chain::output_point outpoint{ block2->transactions()[0].hash(), 0 };
////    const auto expected_value = initial_block_subsidy_satoshi();
////    const auto expected_script = block2->transactions()[0].outputs()[0].script().to_string(0);
////    instance.populate_output(outpoint, 2);
////    BOOST_REQUIRE(outpoint.metadata.cache.is_valid());
////
////    BOOST_REQUIRE(outpoint.metadata.coinbase);
////    BOOST_REQUIRE_EQUAL(outpoint.metadata.height, 2u);
////    BOOST_REQUIRE_EQUAL(outpoint.metadata.cache.value(), expected_value);
////    BOOST_REQUIRE_EQUAL(outpoint.metadata.cache.script().to_string(0), expected_script);
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__populate_output__below_fork__true)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const auto block1 = NEW_BLOCK(1);
////    const auto block2 = NEW_BLOCK(2);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////    BOOST_REQUIRE(instance.push(block2, 2, 0));
////
////    const chain::output_point outpoint{ block2->transactions().front().hash(), 0 };
////    instance.populate_output(outpoint, 3);
////    BOOST_REQUIRE(outpoint.metadata.cache.is_valid());
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__populate_output__at_fork__true)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const auto block1 = NEW_BLOCK(1);
////    const auto block2 = NEW_BLOCK(2);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////    BOOST_REQUIRE(instance.push(block2, 2, 0));
////
////    const chain::output_point outpoint{ block2->transactions().front().hash(), 0 };
////    instance.populate_output(outpoint, 2);
////    BOOST_REQUIRE(outpoint.metadata.cache.is_valid());
////}
////
////BOOST_AUTO_TEST_CASE(block_chain__populate_output__above_fork__false)
////{
////    START_BLOCKCHAIN(instance, false);
////
////    const auto block1 = NEW_BLOCK(1);
////    const auto block2 = NEW_BLOCK(2);
////    BOOST_REQUIRE(instance.push(block1, 1, 0));
////    BOOST_REQUIRE(instance.push(block2, 2, 0));
////
////    const chain::output_point outpoint{ block2->transactions().front().hash(), 0 };
////    instance.populate_output(outpoint, 1);
////    BOOST_REQUIRE(!outpoint.metadata.cache.is_valid());
////}
////
BOOST_AUTO_TEST_SUITE_END()
