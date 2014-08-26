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
#include <bitcoin/format.hpp>
#include <bitcoin/transaction.hpp>
#include <bitcoin/utility/logger.hpp>
#include "simple_chain_impl.hpp"

namespace libbitcoin {
    namespace chain {

bool remove_debit(leveldb::WriteBatch& batch,
    const transaction_input_type& input);
bool remove_credit(leveldb::WriteBatch& batch,
    const transaction_output_type& output, const output_point& outpoint);

simple_chain_impl::simple_chain_impl(
    db_interface& interface,
    blockchain_common_ptr common, leveldb_databases db)
  : interface_(interface), common_(common), db_(db)
{
}

void simple_chain_impl::append(block_detail_ptr incoming_block)
{
    const size_t last_height = interface_.blocks.last_height();
    BITCOIN_ASSERT(last_height != block_database::null_height);
    const block_type& actual_block = incoming_block->actual();
    if (!common_->save_block(last_height + 1, actual_block))
        log_fatal(LOG_BLOCKCHAIN) << "Saving block in organizer failed";
}

int simple_chain_impl::find_index(const hash_digest& search_block_hash)
{
    auto result = interface_.blocks.get(search_block_hash);
    if (!result)
        return -1;
    return static_cast<int>(result.height());
}

hash_number simple_chain_impl::sum_difficulty(size_t begin_index)
{
    hash_number total_work = 0;
    leveldb_iterator it(db_.block->NewIterator(leveldb::ReadOptions()));
    auto raw_height = to_little_endian(begin_index);
    for (it->Seek(slice(raw_height)); it->Valid(); it->Next())
    {
        constexpr size_t bits_field_offset = 4 + 2 * hash_size + 4;
        BITCOIN_ASSERT(it->value().size() >= 84);
        // Deserialize only the bits field of block header.
        const char* bits_field_begin = it->value().data() + bits_field_offset;
        std::string raw_bits(bits_field_begin, 4);
        auto deserial = make_deserializer(raw_bits.begin(), raw_bits.end());
        uint32_t bits = deserial.read_4_bytes();
        // Accumulate the total work.
        total_work += block_work(bits);
    }
    return total_work;
}

block_detail_ptr reconstruct_block(
    blockchain_common_ptr common,
    db_interface& interface_, const std::string& value)
{
    leveldb_block_info blk_info;
    if (!common->deserialize_block(blk_info, value, true, true))
        return nullptr;
    block_detail_ptr blk = std::make_shared<block_detail>(blk_info.header);
    for (const hash_digest& tx_hash: blk_info.tx_hashes)
    {
        // Get the actual transaction.
        auto result = interface_.transactions.get(tx_hash);
        if (!result)
            return nullptr;
        blk->actual_ptr()->transactions.push_back(result.transaction());
    }
    return blk;
}

bool simple_chain_impl::release(size_t begin_index,
    block_detail_list& released_blocks)
{
    leveldb_transaction_batch batch;
    leveldb_iterator it(db_.block->NewIterator(leveldb::ReadOptions()));
    auto raw_height = to_little_endian(begin_index);
    for (it->Seek(slice(raw_height)); it->Valid(); it->Next())
    {
        block_detail_ptr blk =
            reconstruct_block(common_, interface_, it->value().ToString());
        if (!blk)
            return false;
        // Add to list of sliced blocks
        released_blocks.push_back(blk);
        // Make sure to delete hash secondary index too.
        const hash_digest& block_hash = blk->hash();
        // Delete block header...
        batch.block.Delete(it->key());
        // And it's secondary index.
        batch.block_hash.Delete(slice_block_hash(block_hash));
        // Remove txs + spends + addresses too
        const auto& transactions = blk->actual().transactions;
        for (const transaction_type& block_tx: transactions)
            if (!clear_transaction_data(batch, block_tx))
                return false;
    }
    leveldb::WriteOptions options;
    // Execute batches.
    db_.write(batch);
    return true;
}

bool simple_chain_impl::clear_transaction_data(
    leveldb_transaction_batch& batch, const transaction_type& remove_tx)
{
    const hash_digest& tx_hash = hash_transaction(remove_tx);
    batch.tx.Delete(slice(tx_hash));
    // Remove spends
    // ... spends don't exist for coinbase txs.
    if (!is_coinbase(remove_tx))
        for (uint32_t input_index = 0; input_index < remove_tx.inputs.size();
            ++input_index)
        {
            const transaction_input_type& input =
                remove_tx.inputs[input_index];
            // We could check if the spend matches the inpoint for safety.
            //const input_point inpoint{tx_hash, input_index};
            // Recreate the key...
            data_chunk spent_key = create_spent_key(input.previous_output);
            // ... Perform the delete.
            batch.spend.Delete(slice(spent_key));
            if (!remove_debit(batch.debit, input))
                return false;
        }
    // Remove addresses
    for (uint32_t output_index = 0; output_index < remove_tx.outputs.size();
        ++output_index)
    {
        const transaction_output_type& output =
            remove_tx.outputs[output_index];
        if (!remove_credit(batch.credit, output, {tx_hash, output_index}))
            return false;
    }
    return true;
}

bool remove_debit(leveldb::WriteBatch& batch,
    const transaction_input_type& input)
{
    const output_point& outpoint = input.previous_output;
    payment_address address;
    // Not a Bitcoin address so skip this output.
    if (!extract(address, input.script))
        return true;
    data_chunk addr_key = create_address_key(address, outpoint);
    batch.Delete(slice(addr_key));
    return true;
}

bool remove_credit(leveldb::WriteBatch& batch,
    const transaction_output_type& output, const output_point& outpoint)
{
    payment_address address;
    // Not a Bitcoin address so skip this output.
    if (!extract(address, output.script))
        return true;
    data_chunk addr_key = create_address_key(address, outpoint);
    batch.Delete(slice(addr_key));
    return true;
}

    } // namespace chain
} // namespace libbitcoin
