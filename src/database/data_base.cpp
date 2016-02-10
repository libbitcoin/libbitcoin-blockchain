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
#include <bitcoin/blockchain/database/data_base.hpp>

#include <cstdint>
#include <stdexcept>
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/disk/mmfile.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate_block.hpp>

namespace libbitcoin {
namespace database {

using namespace bc::chain;
using namespace bc::wallet;
using namespace boost::filesystem;

// BIP30 exception blocks.
// github.com/bitcoin/bips/blob/master/bip-0030.mediawiki#specification
static const config::checkpoint exception1 =
{ "00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec", 91842 };
static const config::checkpoint exception2 =
{ "00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721", 91880 };

bool data_base::touch_file(const path& file_path)
{
    bc::ofstream file(file_path.string());
    if (file.bad())
        return false;

    // Write one byte so file is nonzero size.
    file.write("H", 1);
    return true;
}

bool data_base::initialize(const path& prefix, const chain::block& genesis)
{
    // Create paths.
    const store paths(prefix);
    const auto result = paths.touch_all();

    // Initialize data_bases.
    data_base instance(paths, 0, 0);
    instance.create();
    instance.start();
    instance.push(genesis);

    return result;
}

data_base::store::store(const path& prefix)
{
    blocks_lookup = prefix / "blocks_lookup";
    blocks_rows = prefix / "blocks_rows";
    spends = prefix / "spends";
    transactions = prefix / "txs";
    history_lookup = prefix / "history_lookup";
    history_rows = prefix / "history_rows";
    stealth_index = prefix / "stealth_index";
    stealth_rows = prefix / "stealth_rows";
    db_lock = prefix / "db_lock";
}

bool data_base::store::touch_all() const
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

data_base::file_lock data_base::initialize_lock(const path& lock)
{
    // Touch the lock file (open/close).
    const auto lock_file_path = lock.string();
    bc::ofstream file(lock_file_path, std::ios::app);
    file.close();
    return file_lock(lock_file_path.c_str());
}

data_base::data_base(const settings& settings)
  : data_base(settings.database_path, settings.history_start_height,
        settings.stealth_start_height)
{
}

data_base::data_base(const path& prefix, size_t history_height,
    size_t stealth_height)
  : data_base(store(prefix), history_height, stealth_height)
{
}

data_base::data_base(const store& paths, size_t history_height,
    size_t stealth_height)
  : blocks(paths.blocks_lookup, paths.blocks_rows),
    spends(paths.spends),
    transactions(paths.transactions),
    history(paths.history_lookup, paths.history_rows),
    stealth(paths.stealth_index, paths.stealth_rows),
    history_height_(history_height),
    stealth_height_(stealth_height),
    file_lock_(initialize_lock(paths.db_lock)),
    sequential_lock_(0)
{
}

// ----------------------------------------------------------------------------
// Startup and shutdown.

bool data_base::create()
{
    blocks.create();
    spends.create();
    transactions.create();
    history.create();
    stealth.create();
    
    // TODO: detect and return failure condition.
    return true;
}

bool data_base::start()
{
    if (!file_lock_.try_lock())
        return false;

    blocks.start();
    spends.start();
    transactions.start();
    history.start();
    stealth.start();

    // TODO: detect and return failure condition.
    return true;
}

bool data_base::stop()
{
    // TODO: close file descriptors and return result.
    return true;
}

// ----------------------------------------------------------------------------
// Locking.

handle data_base::start_read()
{
    return sequential_lock_.load();
}

bool data_base::is_read_valid(handle value)
{
    return value == sequential_lock_;
}

bool data_base::is_write_locked(handle value)
{
    return (value % 2) == 1;
}

bool data_base::start_write()
{
    // slock is now odd.
    return is_write_locked(++sequential_lock_);
}

bool data_base::end_write()
{
    // slock_ is now even again.
    return !is_write_locked(++sequential_lock_);
}

// ----------------------------------------------------------------------------
// Query engines.

static size_t get_next_height(const block_database& blocks)
{
    size_t current_height;
    const auto empty_chain = !blocks.top(current_height);
    return empty_chain ? 0 : current_height + 1;
}

static bool is_allowed_duplicate(const header& head, size_t height)
{
    return
        (height == exception1.height() && head.hash() == exception1.hash()) ||
        (height == exception2.height() && head.hash() == exception2.hash());
}

void data_base::synchronize()
{
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

void data_base::push(const block& block)
{
    const auto height = get_next_height(blocks);

    for (size_t index = 0; index < block.transactions.size(); ++index)
    {
        // Skip BIP30 allowed duplicates (coinbase txs of excepted blocks).
        // We handle here because this is the lowest public level exposed.
        if (index == 0 && is_allowed_duplicate(block.header, height))
            continue;

        const auto& tx = block.transactions[index];
        const auto tx_hash = tx.hash();

        // Add inputs
        if (!tx.is_coinbase())
            push_inputs(tx_hash, height, tx.inputs);

        // Add outputs
        push_outputs(tx_hash, height, tx.outputs);

        // Add stealth outputs
        push_stealth(tx_hash, height, tx.outputs);

        // Add transaction
        transactions.store(height, index, tx);
    }

    // Add block itself.
    blocks.store(block);

    // Synchronise everything that was added.
    synchronize();
}

void data_base::push_inputs(const hash_digest& tx_hash, size_t height,
    const input::list& inputs)
{
    for (uint32_t index = 0; index < inputs.size(); ++index)
    {
        const auto& input = inputs[index];
        const chain::input_point spend{ tx_hash, index };
        spends.store(input.previous_output, spend);

        if (height < history_height_)
            continue;

        // Try to extract an address.
        const auto address = payment_address::extract(input.script);
        if (!address)
            continue;

        history.add_spend(address.hash(), input.previous_output, spend,
            height);
    }
}

void data_base::push_outputs(const hash_digest& tx_hash, size_t height,
    const output::list& outputs)
{
    if (height < history_height_)
        return;

    for (uint32_t index = 0; index < outputs.size(); ++index)
    {
        const auto& output = outputs[index];
        const chain::output_point outpoint{ tx_hash, index };

        // Try to extract an address.
        const auto address = payment_address::extract(output.script);
        if (!address)
            continue;

        history.add_output(address.hash(), outpoint, height,
            output.value);
    }
}

void data_base::push_stealth(const hash_digest& tx_hash, size_t height,
    const output::list& outputs)
{
    if (height < stealth_height_)
        return;

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

        const chain::stealth_row row
        {
            unsigned_ephemeral_key,
            address.hash(),
            tx_hash
        };

        stealth.store(prefix, row);
    }
}

chain::block data_base::pop()
{
    size_t height;
    if (!blocks.top(height))
        throw std::runtime_error("The blockchain is empty.");

    auto block_result = blocks.get(height);

    // Set result header.
    chain::block block;
    block.header = block_result.header();
    const auto count = block_result.transactions_size();

    // TODO: unreverse the loop so we can avoid this.
    BITCOIN_ASSERT_MSG(count <= max_int64, "overflow");
    const auto unsigned_count = static_cast<int64_t>(count);

    // Loop backwards (in reverse to how we added).
    for (int64_t index = unsigned_count - 1; index >= 0; --index)
    {
        const auto tx_hash = block_result.transaction_hash(index);
        auto tx_result = transactions.get(tx_hash);
        BITCOIN_ASSERT(tx_result);
        BITCOIN_ASSERT(tx_result.height() == height);
        BITCOIN_ASSERT(tx_result.index() == static_cast<size_t>(index));

        const auto tx = tx_result.transaction();

        // Do things in reverse so pop txs, then outputs, then inputs.
        transactions.remove(tx_hash);

        // Remove outputs
        pop_outputs(tx.outputs, height);

        // Remove inputs
        if (!tx.is_coinbase())
            pop_inputs(tx.inputs, height);

        // TODO: need to pop stealth, referencing deleted txs.

        // Add transaction to result
        block.transactions.push_back(tx);
    }

    stealth.unlink(height);
    blocks.unlink(height);

    // Synchronise everything that was changed.
    synchronize();

    // Reverse, since we looped backwards.
    std::reverse(block.transactions.begin(), block.transactions.end());
    return block;
}

void data_base::pop_inputs(const input::list& inputs, size_t height)
{
    BITCOIN_ASSERT_MSG(inputs.size() <= max_int64, "overflow");
    const auto inputs_size = static_cast<int64_t>(inputs.size());

    // Loop in reverse.
    for (int64_t index = inputs_size - 1; index >= 0; --index)
    {
        const auto& input = inputs[index];
        spends.remove(input.previous_output);

        if (height < history_height_)
            continue;

        // Try to extract an address.
        const auto address = payment_address::extract(input.script);
        if (address)
            history.delete_last_row(address.hash());
    }
}

void data_base::pop_outputs(const output::list& outputs, size_t height)
{
    if (height < history_height_)
        return;

    BITCOIN_ASSERT_MSG(outputs.size() <= max_int64, "overflow");
    const auto outputs_size = static_cast<int64_t>(outputs.size());

    // Loop in reverse.
    for (int64_t index = outputs_size - 1; index >= 0; --index)
    {
        const auto& output = outputs[index];

        // Try to extract an address.
        const auto address = payment_address::extract(output.script);
        if (address)
            history.delete_last_row(address.hash());
    }
}

} // namespace data_base
} // namespace libbitcoin
