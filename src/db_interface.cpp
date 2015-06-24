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
#include <bitcoin/blockchain/db_interface.hpp>

#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>

namespace libbitcoin {
namespace blockchain {

using boost::filesystem::path;

bool touch_file(const path& filepath)
{
    bc::ofstream file(filepath.string());
    if (file.bad())
        return false;

    // Write one byte so file is nonzero size.
    file.write("H", 1);
    return true;
}

bool initialize_blockchain(const path& prefix)
{
    // Create paths.
    db_paths paths(prefix);
    bool result = paths.touch_all();

    // Initialize databases.
    db_interface database(paths, { 0 });
    database.create();

    return result;
}

db_paths::db_paths(const path& prefix)
{
    blocks_lookup = prefix / "blocks_lookup";
    blocks_rows = prefix / "blocks_rows";
    spends = prefix / "spends";
    transactions = prefix / "txs";
    history_lookup = prefix / "history_lookup";
    history_rows = prefix / "history_rows";
    stealth_index = prefix / "stealth_index";
    stealth_rows = prefix / "stealth_rows";
}

bool db_paths::touch_all() const
{
    return
        touch_file(blocks_lookup) &&
        touch_file(blocks_rows) &&
        touch_file(spends) &&
        touch_file(transactions) &&
        touch_file(history_lookup) &&
        touch_file(history_rows) &&
        touch_file(stealth_index) &&
        touch_file(stealth_rows);
}

db_interface::db_interface(const db_paths& paths,
    const db_active_heights &active_heights)
  : blocks(paths.blocks_lookup, paths.blocks_rows),
    spends(paths.spends),
    transactions(paths.transactions),
    history(paths.history_lookup, paths.history_rows),
    stealth(paths.stealth_index, paths.stealth_rows),
    active_heights_(active_heights)
{
}

void db_interface::create()
{
    blocks.create();
    spends.create();
    transactions.create();
    history.create();
    stealth.create();
}

void db_interface::start()
{
    blocks.start();
    spends.start();
    transactions.start();
    history.start();
    stealth.start();
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

void db_interface::push(const chain::block& block)
{
    const auto block_height = next_height(blocks.last_height());
    for (size_t i = 0; i < block.transactions.size(); ++i)
    {
        const auto& tx = block.transactions[i];
        const transaction_metainfo info{block_height, i};

        // Skip special duplicate transactions.
        if (is_special_duplicate(info))
            continue;

        const auto tx_hash = tx.hash();

        // Add inputs
        if (!tx.is_coinbase())
            push_inputs(tx_hash, block_height, tx.inputs);

        // Add outputs
        push_outputs(tx_hash, block_height, tx.outputs);

        // Add stealth outputs
        push_stealth_outputs(tx_hash, tx.outputs);

        // Add transaction
        transactions.store(info, tx);
    }

    // Add block itself.
    blocks.store(block);

    // Synchronise everything...
    spends.sync();
    transactions.sync();
    history.sync();
    stealth.sync();

    // ... do block header last so if there's a crash midway
    // then on the next startup we'll try to redownload the
    // last block and it will fail because blockchain was left
    // in an inconsistent state.
    blocks.sync();
}

chain::block db_interface::pop()
{
    chain::block result;
    const auto block_height = blocks.last_height();
    auto block_result = blocks.get(block_height);
    BITCOIN_ASSERT(block_result);

    // Set result header
    result.header = block_result.header();
    const auto txs_size = block_result.transactions_size();

    // Loop backwards (in reverse to how we added).
    for (int i = txs_size - 1; i >= 0; --i)
    {
        const auto tx_hash = block_result.transaction_hash(i);
        auto tx_result = transactions.get(tx_hash);
        BITCOIN_ASSERT(tx_result);
        BITCOIN_ASSERT(tx_result.height() == block_height);
        BITCOIN_ASSERT(tx_result.index() == static_cast<size_t>(i));

        const auto tx = tx_result.transaction();

        // Do things in reverse so pop txs, then outputs, then inputs.
        transactions.remove(tx_hash);

        // Remove outputs
        pop_outputs(block_height, tx.outputs);

        // Remove inputs
        if (!tx.is_coinbase())
            pop_inputs(block_height, tx.inputs);

        // Add transaction to result
        result.transactions.push_back(tx);
    }

    stealth.unlink(block_height);
    blocks.unlink(block_height);

    // Since we looped backwards
    std::reverse(result.transactions.begin(), result.transactions.end());
    return result;
}

void db_interface::push_inputs(const hash_digest& tx_hash,
    const size_t block_height, const chain::transaction_input::list& inputs)
{
    for (uint32_t i = 0; i < inputs.size(); ++i)
    {
        const auto& input = inputs[i];
        const chain::input_point spend{tx_hash, i};
        spends.store(input.previous_output, spend);

        // Skip history if not at the right level yet.
        if (block_height < active_heights_.history)
            continue;

        // Try to extract an address.
        wallet::payment_address address;
        if (!extract(address, input.script))
            continue;

        history.add_spend(address.hash(),
            input.previous_output, spend, block_height);
    }
}

void db_interface::push_outputs(const hash_digest& tx_hash,
    const size_t block_height, const chain::transaction_output::list& outputs)
{
    for (uint32_t i = 0; i < outputs.size(); ++i)
    {
        const auto& output = outputs[i];
        const chain::output_point outpoint{tx_hash, i};

        // Skip history if not at the right level yet.
        if (block_height < active_heights_.history)
            continue;

        // Try to extract an address.
        wallet::payment_address address;
        if (!extract(address, output.script))
            continue;

        history.add_output(address.hash(),
            outpoint, block_height, output.value);
    }
}

hash_digest read_ephemkey(const data_chunk& stealth_data)
{
    // Read ephemkey
    hash_digest ephemkey;
    BITCOIN_ASSERT(stealth_data.size() >= hash_size);
    std::copy(stealth_data.begin(), stealth_data.begin() + hash_size,
        ephemkey.begin());
    return ephemkey;
}

void db_interface::push_stealth_outputs(const hash_digest& tx_hash,
    const chain::transaction_output::list& outputs)
{
    // Stealth cannot be in last output because there needs
    // to be a matching following output.
    const int last_possible = outputs.size() - 1;
    for (int i = 0; i < last_possible; ++i)
    {
        const auto& output = outputs[i];
        const auto& next_output = outputs[i + 1];

        // Skip past output if not stealth data.
        if (output.script.type() != chain::payment_type::stealth_info)
            continue;

        // Try to extract an address.
        wallet::payment_address address;
        if (!extract(address, next_output.script))
            continue;

        // Stealth data.
        BITCOIN_ASSERT(output.script.operations.size() == 2);
        const auto& stealth_data = output.script.operations[1].data;
        stealth_row row
        {
            read_ephemkey(stealth_data),
            address.hash(),
            tx_hash
        };
        stealth.store(output.script, row);
    }
}

void db_interface::pop_inputs(const size_t block_height,
    const chain::transaction_input::list& inputs)
{
    // Loop in reverse.
    for (int i = inputs.size() - 1; i >= 0; --i)
    {
        const auto& input = inputs[i];
        spends.remove(input.previous_output);

        // Skip history if not at the right level yet.
        if (block_height < active_heights_.history)
            continue;

        // Try to extract an address.
        wallet::payment_address address;

        if (!extract(address, input.script))
            continue;

        history.delete_last_row(address.hash());
    }
}

void db_interface::pop_outputs(const size_t block_height,
    const chain::transaction_output::list& outputs)
{
    // Loop in reverse.
    for (int i = outputs.size() - 1; i >= 0; --i)
    {
        const auto& output = outputs[i];

        // Skip history if not at the right level yet.
        if (block_height < active_heights_.history)
            continue;

        // Try to extract an address.
        wallet::payment_address address;

        if (!extract(address, output.script))
            continue;

        history.delete_last_row(address.hash());
    }
}

} // namespace blockchain
} // namespace libbitcoin
