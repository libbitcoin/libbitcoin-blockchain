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
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

BOOST_AUTO_TEST_SUITE(interface_test)

void test_block_exists(
    const db_interface& interface, const block_type block0)
{
    const size_t last_height = interface.blocks.last_height();
    const hash_digest blk_hash = hash_block_header(block0.header);
    auto r0 = interface.blocks.get(last_height);
    auto r0_byhash = interface.blocks.get(blk_hash);
    BOOST_REQUIRE(r0);
    BOOST_REQUIRE(r0_byhash);
    BOOST_REQUIRE(hash_block_header(r0.header()) == blk_hash);
    BOOST_REQUIRE(hash_block_header(r0_byhash.header()) == blk_hash);
    BOOST_REQUIRE(r0.height() == last_height);
    BOOST_REQUIRE(r0_byhash.height() == last_height);
    BOOST_REQUIRE(r0.transactions_size() == block0.transactions.size());
    BOOST_REQUIRE(r0_byhash.transactions_size() == block0.transactions.size());
    for (size_t i = 0; i < block0.transactions.size(); ++i)
    {
        const transaction_type& tx = block0.transactions[i];
        const hash_digest tx_hash = hash_transaction(tx);
        const index_type idx = r0.transaction_index(i);
        BOOST_REQUIRE(r0_byhash.transaction_index(i) == idx);
        auto r0_tx = interface.transactions.get(idx);
        auto r0_tx_byhash = interface.transactions.get(tx_hash);
        BOOST_REQUIRE(r0_tx);
        BOOST_REQUIRE(r0_byhash);
        BOOST_REQUIRE(hash_transaction(r0_tx.transaction()) == tx_hash);
        BOOST_REQUIRE(hash_transaction(r0_tx_byhash.transaction()) == tx_hash);
        BOOST_REQUIRE(r0_tx.height() == last_height);
        BOOST_REQUIRE(r0_tx_byhash.height() == last_height);
        BOOST_REQUIRE(r0_tx.index() == i);
        BOOST_REQUIRE(r0_tx_byhash.index() == i);

        if (!is_coinbase(tx))
        {
            for (size_t j = 0; j < tx.inputs.size(); ++j)
            {
                const transaction_input_type& input = tx.inputs[j];
                input_point spend{tx_hash, static_cast<uint32_t>(j)};
                auto r0_spend = interface.spends.get(input.previous_output);
                BOOST_REQUIRE(r0_spend);
                BOOST_REQUIRE(r0_spend.hash() == spend.hash);
                BOOST_REQUIRE(r0_spend.index() == spend.index);

                payment_address address;
                if (!extract(address, input.script))
                    continue;
                auto r_history = interface.history.get(address.hash());
                bool found = false;
                for (const auto row: r_history.history)
                {
                    if (row.spend.hash == spend.hash &&
                        row.spend.index == spend.index)
                    {
                        BOOST_REQUIRE(row.spend_height == last_height);
                        found = true;
                        break;
                    }
                }
                BOOST_REQUIRE(found);
            }
        }
        for (size_t j = 0; j < tx.outputs.size(); ++j)
        {
            const transaction_output_type& output = tx.outputs[j];
            output_point outpoint{tx_hash, static_cast<uint32_t>(j)};

            payment_address address;
            if (!extract(address, output.script))
                continue;
            auto r_history = interface.history.get(address.hash());
            bool found = false;
            for (const auto row: r_history.history)
            {
                if (row.output.hash == outpoint.hash &&
                    row.output.index == outpoint.index)
                {
                    BOOST_REQUIRE(row.output_height == last_height);
                    BOOST_REQUIRE(row.value == output.value);
                    found = true;
                    break;
                }
            }
            BOOST_REQUIRE(found);
        }
    }
}

void test_block_not_exists(
    const db_interface& interface, const block_type block0)
{
    const hash_digest blk_hash = hash_block_header(block0.header);
    auto r0_byhash = interface.blocks.get(blk_hash);
    BOOST_REQUIRE(!r0_byhash);
    for (size_t i = 0; i < block0.transactions.size(); ++i)
    {
        const transaction_type& tx = block0.transactions[i];
        const hash_digest tx_hash = hash_transaction(tx);

        if (!is_coinbase(tx))
        {
            for (size_t j = 0; j < tx.inputs.size(); ++j)
            {
                const transaction_input_type& input = tx.inputs[j];
                input_point spend{tx_hash, static_cast<uint32_t>(j)};
                auto r0_spend = interface.spends.get(input.previous_output);
                BOOST_REQUIRE(!r0_spend);

                payment_address address;
                if (!extract(address, input.script))
                    continue;
                auto r_history = interface.history.get(address.hash());
                bool found = false;
                for (const auto row: r_history.history)
                {
                    if (row.spend.hash == spend.hash &&
                        row.spend.index == spend.index)
                    {
                        found = true;
                        break;
                    }
                }
                BOOST_REQUIRE(!found);
            }
        }
        for (size_t j = 0; j < tx.outputs.size(); ++j)
        {
            const transaction_output_type& output = tx.outputs[j];
            output_point outpoint{tx_hash, static_cast<uint32_t>(j)};

            payment_address address;
            if (!extract(address, output.script))
                continue;
            auto r_history = interface.history.get(address.hash());
            bool found = false;
            for (const auto row: r_history.history)
            {
                if (row.output.hash == outpoint.hash &&
                    row.output.index == outpoint.index)
                {
                    found = true;
                    break;
                }
            }
            BOOST_REQUIRE(!found);
        }
    }
}

BOOST_AUTO_TEST_CASE(pushpop)
{
    const std::string prefix = "chain";
    boost::filesystem::create_directory(prefix);
    initialize_blockchain(prefix);

    db_paths paths(prefix);
    db_interface interface(paths);
    interface.start();

    BOOST_REQUIRE(interface.blocks.last_height() ==
        block_database::null_height);

    block_type block0 = genesis_block();
    test_block_not_exists(interface, block0);
    interface.push(block0);
    test_block_exists(interface, block0);
}

BOOST_AUTO_TEST_SUITE_END()

