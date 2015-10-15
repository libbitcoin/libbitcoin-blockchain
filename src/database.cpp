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
#include <bitcoin/blockchain/database.hpp>

#include <cstdint>
#include <stdexcept>
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace wallet;
using boost::filesystem::path;

bool database::touch_file(const path& filepath)
{
    bc::ofstream file(filepath.string());
    if (file.bad())
        return false;

    // Write one byte so file is nonzero size.
    file.write("H", 1);
    return true;
}

bool database::initialize(const path& prefix, const chain::block& genesis)
{
    // Create paths.
    const store paths(prefix);
    const auto result = paths.touch_all();

    // Initialize databases.
    database instance(paths, 0);
    instance.create();
    instance.start();
    instance.push(genesis);

    return result;
}

database::store::store(const path& prefix)
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

bool database::store::touch_all() const
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

database::database(const store& paths, size_t history_height)
  : blocks(paths.blocks_lookup, paths.blocks_rows),
    spends(paths.spends),
    transactions(paths.transactions),
    history(paths.history_lookup, paths.history_rows),
    stealth(paths.stealth_index, paths.stealth_rows),
    history_height_(history_height)
{
}

void database::create()
{
    blocks.create();
    spends.create();
    transactions.create();
    history.create();
    stealth.create();
}

void database::start()
{
    blocks.start();
    spends.start();
    transactions.start();
    history.start();
    stealth.start();
}

static size_t get_next_height(const block_database& blocks)
{
    size_t current_height;
    const auto empty_chain = !blocks.top(current_height);
    return empty_chain ? 0 : current_height + 1;
}

// There are 2 duplicated transactions in the blockchain.
// Since then this part of Bitcoin was changed to disallow duplicates.
static bool is_special_duplicate(const transaction_metainfo& metadata)
{
    return (metadata.height == 91842 || metadata.height == 91880) &&
        metadata.index == 0;
}

void database::push(const chain::block& block)
{
    const auto block_height = get_next_height(blocks);

    for (size_t index = 0; index < block.transactions.size(); ++index)
    {
        const auto& tx = block.transactions[index];
        const transaction_metainfo metadata
        {
            block_height,
            index
        };

        // Skip special duplicate transactions.
        if (is_special_duplicate(metadata))
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
        transactions.store(metadata, tx);
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

chain::block database::pop()
{
    chain::block result;
    size_t block_height;

    if (!blocks.top(block_height))
        throw std::runtime_error("The blockchain is empty.");

    auto block_result = blocks.get(block_height);

    // Set result header.
    result.header = block_result.header();
    const auto txs_size = block_result.transactions_size();

    // TODO: unreverse the loop so we can avoid this.
    BITCOIN_ASSERT_MSG(txs_size <= max_int64, "overflow");
    const auto unsigned_txs_size = static_cast<int64_t>(txs_size);

    // Loop backwards (in reverse to how we added).
    for (int64_t index = unsigned_txs_size - 1; index >= 0; --index)
    {
        const auto tx_hash = block_result.transaction_hash(index);
        auto tx_result = transactions.get(tx_hash);
        BITCOIN_ASSERT(tx_result);
        BITCOIN_ASSERT(tx_result.height() == block_height);
        BITCOIN_ASSERT(tx_result.index() == static_cast<size_t>(index));

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

    // Reverse, since we looped backwards.
    std::reverse(result.transactions.begin(), result.transactions.end());
    return result;
}

void database::push_inputs(const hash_digest& tx_hash,
    const size_t block_height, const chain::input::list& inputs)
{
    for (uint32_t index = 0; index < inputs.size(); ++index)
    {
        const auto& input = inputs[index];
        const chain::input_point spend{ tx_hash, index };
        spends.store(input.previous_output, spend);

        // Skip history if not at the right level yet.
        if (block_height < history_height_)
            continue;

        // Try to extract an address.
        const auto address = payment_address::extract(input.script);
        if (!address)
            continue;

        history.add_spend(address.hash(), input.previous_output, spend,
            block_height);
    }
}

void database::push_outputs(const hash_digest& tx_hash,
    const size_t block_height, const chain::output::list& outputs)
{
    for (uint32_t index = 0; index < outputs.size(); ++index)
    {
        const auto& output = outputs[index];
        const chain::output_point outpoint{ tx_hash, index };

        // Skip history if not at the right level yet.
        if (block_height < history_height_)
            continue;

        // Try to extract an address.
        const auto address = payment_address::extract(output.script);
        if (!address)
            continue;

        history.add_output(address.hash(), outpoint, block_height,
            output.value);
    }
}

void database::push_stealth_outputs(const hash_digest& tx_hash,
    const chain::output::list& outputs)
{
    // TODO: unreverse the loop so we can avoid this.
    BITCOIN_ASSERT_MSG(outputs.size() <= max_int64, "overflow");
    const auto outputs_size = static_cast<int64_t>(outputs.size());

    // Stealth cannot be in last output because it is paired.
    for (int64_t index = 0; index < (outputs_size - 1); ++index)
    {
        const auto& ephemeral_script = outputs[index].script;
        const auto& payment_script = outputs[index + 1].script;

        // Try to extract an unsigned ephemeral key from the odd output.
        hash_digest unsigned_ephemeral_key;
        if (!extract_ephemeral_key(unsigned_ephemeral_key, ephemeral_script))
            continue;

        // Try to extract a stealth prefix from the odd output.
        uint32_t prefix;
        if (!to_stealth_prefix(prefix, ephemeral_script))
            continue;

        // Try to extract the payment address from the even output.
        // The payment address versions are arbitrary and unused here.
        const auto address = payment_address::extract(payment_script);
        if (!address)
            continue;

        const block_chain::stealth_row row
        {
            unsigned_ephemeral_key,
            address.hash(),
            tx_hash
        };

        stealth.store(prefix, row);
    }
}

void database::pop_inputs(const size_t block_height,
    const chain::input::list& inputs)
{
    // TODO: unreverse the loop so we can avoid this.
    BITCOIN_ASSERT_MSG(inputs.size() <= max_int64, "overflow");
    const auto inputs_size = static_cast<int64_t>(inputs.size());

    // Loop in reverse.
    for (int64_t index = inputs_size - 1; index >= 0; --index)
    {
        const auto& input = inputs[index];
        spends.remove(input.previous_output);

        // Skip history if not at the right level yet.
        if (block_height < history_height_)
            continue;

        // Try to extract an address.
        const auto address = payment_address::extract(input.script);
        if (address)
            history.delete_last_row(address.hash());
    }
}

void database::pop_outputs(const size_t block_height,
    const chain::output::list& outputs)
{
    // TODO: unreverse the loop so we can avoid this.
    BITCOIN_ASSERT_MSG(outputs.size() <= max_int64, "overflow");
    const auto outputs_size = static_cast<int64_t>(outputs.size());

    // Loop in reverse.
    for (int64_t index = outputs_size - 1; index >= 0; --index)
    {
        const auto& output = outputs[index];

        // Skip history if not at the right level yet.
        if (block_height < history_height_)
            continue;

        // Try to extract an address.
        const auto address = payment_address::extract(output.script);
        if (address)
            history.delete_last_row(address.hash());
    }
}

} // namespace blockchain
} // namespace libbitcoin
