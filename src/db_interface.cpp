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
#include <bitcoin/blockchain/db_interface.hpp>

#include <boost/filesystem.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

const std::string path(const std::string& prefix, const std::string& filename)
{
    using boost::filesystem::path;
    path db_path = path(prefix) / filename;
    return db_path.generic_string();
}

db_paths::db_paths(const std::string& prefix)
{
    blocks_lookup = path(prefix, "blocks_lookup");
    blocks_rows = path(prefix, "blocks_rows");
    spends = path(prefix, "spends");
    transaction_map = path(prefix, "tx_map");

    history_lookup = path(prefix, "history_lookup");
    history_rows = path(prefix, "history_rows");
}

void db_paths::touch_all() const
{
    touch_file(blocks_lookup);
    touch_file(blocks_rows);
    touch_file(spends);
    touch_file(transaction_map);
    touch_file(history_lookup);
    touch_file(history_rows);
}

db_interface::db_interface(const db_paths& paths)
  : blocks(paths.blocks_lookup, paths.blocks_rows),
    spends(paths.spends),
    transactions(paths.transaction_map),
    history(paths.history_lookup, paths.history_rows)
{
}

void db_interface::initialize_new()
{
    blocks.initialize_new();
    spends.initialize_new();
    transactions.initialize_new();
    history.initialize_new();
}

void db_interface::start()
{
    blocks.start();
    spends.start();
    transactions.start();
    history.start();
}

size_t next_height(const size_t current_height)
{
    if (current_height == block_database::null_height)
        return 0;
    return current_height + 1;
}

// There are 2 duplicated transactions in the blockchain.
// Since then this part of Bitcoin was changed to disallow duplicates.
bool is_special_duplicate(const transaction_metainfo& info)
{
    return (info.height == 91842 || info.height == 91880) &&
        info.index == 0;
}

void db_interface::push(const block_type& block)
{
    const size_t block_height = next_height(blocks.last_height());
    for (size_t i = 0; i < block.transactions.size(); ++i)
    {
        const transaction_type& tx = block.transactions[i];
        const transaction_metainfo info{block_height, i};
        // Skip special duplicate transactions.
        if (is_special_duplicate(info))
            continue;
        const hash_digest tx_hash = hash_transaction(tx);
        // Add inputs
        if (!is_coinbase(tx))
            push_inputs(tx_hash, block_height, tx.inputs);
        // Add outputs
        push_outputs(tx_hash, block_height, tx.outputs);
        // Add transaction
        transactions.store(info, tx);
    }
    // Add block itself.
    blocks.store(block);
    // Synchronise everything...
    spends.sync();
    history.sync();
    transactions.sync();
    // ... do block header last so if there's a crash midway
    // then on the next startup we'll try to redownload the
    // last block and it will fail because blockchain was left
    // in an inconsistent state.
    blocks.sync();
}

block_type db_interface::pop()
{
    block_type result;
    const size_t block_height = blocks.last_height();
    auto block_result = blocks.get(block_height);
    BITCOIN_ASSERT(block_result);
    // Set result header
    result.header = block_result.header();
    const size_t txs_size = block_result.transactions_size();
    // Loop backwards (in reverse to how we added).
    for (int i = txs_size - 1; i >= 0; --i)
    {
        const hash_digest tx_hash = block_result.transaction_hash(i);
        auto tx_result = transactions.get(tx_hash);
        BITCOIN_ASSERT(tx_result);
        BITCOIN_ASSERT(tx_result.height() == block_height);
        BITCOIN_ASSERT(tx_result.index() == static_cast<size_t>(i));
        const transaction_type tx = tx_result.transaction();
        // Remove inputs
        if (!is_coinbase(tx))
            pop_inputs(tx_hash, tx.inputs);
        // Remove outputs
        pop_outputs(tx.outputs);
        // Add transaction to result
        result.transactions.push_back(tx);
    }
    blocks.unlink(block_height);
    // Since we looped backwards
    std::reverse(result.transactions.begin(), result.transactions.end());
    return result;
}

void db_interface::push_inputs(
    const hash_digest& tx_hash, const size_t block_height,
    const transaction_input_list& inputs)
{
    for (uint32_t i = 0; i < inputs.size(); ++i)
    {
        const transaction_input_type& input = inputs[i];
        const input_point spend{tx_hash, i};
        spends.store(input.previous_output, spend);
        // Try to extract an address.
        payment_address address;
        if (!extract(address, input.script))
            continue;
        history.add_spend(address.hash(),
            input.previous_output, spend, block_height);
    }
}

void db_interface::push_outputs(
    const hash_digest& tx_hash, const size_t block_height,
    const transaction_output_list& outputs)
{
    for (uint32_t i = 0; i < outputs.size(); ++i)
    {
        const transaction_output_type& output = outputs[i];
        const output_point outpoint{tx_hash, i};
        // Try to extract an address.
        payment_address address;
        if (!extract(address, output.script))
            continue;
        history.add_row(address.hash(),
            outpoint, block_height, output.value);
    }
}

void db_interface::pop_inputs(
    const hash_digest& tx_hash,
    const transaction_input_list& inputs)
{
    for (int i = inputs.size() - 1; i >= 0; --i)
    {
        const transaction_input_type& input = inputs[i];
        const input_point spend{tx_hash, static_cast<uint32_t>(i)};
        spends.remove(input.previous_output);
        // Try to extract an address.
        payment_address address;
        if (!extract(address, input.script))
            continue;
        history.delete_spend(address.hash(), spend);
    }
}

void db_interface::pop_outputs(
    const transaction_output_list& outputs)
{
    for (int i = outputs.size() - 1; i >= 0; --i)
    {
        const transaction_output_type& output = outputs[i];
        // Try to extract an address.
        payment_address address;
        if (!extract(address, output.script))
            continue;
        history.delete_last_row(address.hash());
    }
}

void initialize_blockchain(const std::string& prefix)
{
    db_paths paths(prefix);
    paths.touch_all();
    db_interface interface(paths);
    interface.initialize_new();
}

    } // namespace chain
} // namespace libbitcoin

