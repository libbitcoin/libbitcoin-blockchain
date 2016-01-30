/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/transaction_pool_index.hpp>

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/block_chain.hpp>

namespace libbitcoin {
namespace blockchain {

#define NAME "index"
    
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::network;
using namespace bc::wallet;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

static constexpr uint64_t genesis_height = 0;

transaction_pool_index::transaction_pool_index(threadpool& pool,
    block_chain& blockchain)
  : dispatch_(pool, NAME),
    blockchain_(blockchain)
{
}

// ----------------------------------------------------------------------------
// Add sequence.

void transaction_pool_index::add(const transaction& tx,
    completion_handler handler)
{
    dispatch_.ordered(
        std::bind(&transaction_pool_index::do_add,
            this, tx, handler));
}

void transaction_pool_index::do_add(const transaction& tx,
    completion_handler handler)
{
    const auto tx_hash = tx.hash();

    uint32_t index = 0;
    for (const auto& input: tx.inputs)
    {
        const auto address = payment_address::extract(input.script);
        if (address)
        {
            const input_point point{ tx_hash, index };
            const spend_info info{ point, input.previous_output };
            spends_map_.emplace(address, info);
        }

        ++index;
    }

    index = 0;
    for (const auto& output: tx.outputs)
    {
        const auto address = payment_address::extract(output.script);
        if (address)
        {
            const output_point point{ tx_hash, index };
            const output_info info{ point, output.value };
            outputs_map_.emplace(address, info);
        }

        ++index;
    }

    // This is the end of the add sequence.
    handler(error::success);
}

// ----------------------------------------------------------------------------
// Remove sequence.

void transaction_pool_index::remove(const transaction& tx,
    completion_handler handler)
{
    dispatch_.ordered(
        std::bind(&transaction_pool_index::do_remove,
            this, tx, handler));
}

template <typename Point, typename Multimap>
auto find(const payment_address& key, const Point& value_point,
    Multimap& map) -> decltype(map.begin())
{
    // The entry should only occur once in the multimap.
    const auto range = map.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
        if (it->second.point == value_point)
            return it;

    return map.end();
}

void transaction_pool_index::do_remove(const transaction& tx,
    completion_handler handler)
{
    const auto tx_hash = tx.hash();

    uint32_t index = 0;
    for (const auto& input: tx.inputs)
    {
        const auto address = payment_address::extract(input.script);
        if (address)
        {
            const input_point point{ tx_hash, index };
            const auto entry = find(address, point, spends_map_);
            spends_map_.erase(entry);
        }

        ++index;
    }

    index = 0;
    for (const auto& output: tx.outputs)
    {
        const auto address = payment_address::extract(output.script);
        if (address)
        {
            const output_point point{ tx_hash, index };
            const auto entry = find(address, point, outputs_map_);
            outputs_map_.erase(entry);
        }

        ++index;
    }

    // This is the end of the remove sequence.
    handler(error::success);
}

// ----------------------------------------------------------------------------
// Fetch all history sequence.

// Fetch the history first from the blockchain and then from the tx pool index.
void transaction_pool_index::fetch_all_history(const payment_address& address,
    size_t limit, size_t from_height, fetch_handler handler)
{
    blockchain_.fetch_history(address, limit, from_height,
        std::bind(&transaction_pool_index::blockchain_history_fetched,
            this, _1, _2, address, handler));
}

void transaction_pool_index::blockchain_history_fetched(const code& ec,
    const block_chain::history& history, const payment_address& address,
    fetch_handler handler)
{
    if (ec)
    {
        handler(ec, history);
        return;
    }

    fetch_index_history(address,
        std::bind(&transaction_pool_index::index_history_fetched,
            _1, _2, _3, history, handler));
}

void transaction_pool_index::index_history_fetched(const code& ec,
    const spend_info::list& spends, const output_info::list& outputs,
    block_chain::history history, fetch_handler handler)
{
    if (ec)
    {
        handler(ec, history);
        return;
    }

    // Race conditions raise the possiblity of seeing a spend or output more
    // than once. We collapse any duplicates here and continue.
    add(history, spends);
    add(history, outputs);

    // This is the end of the fetch_all_history sequence.
    handler(error::success, history);
}

// ----------------------------------------------------------------------------
// Fetch index history sequence.

// Fetch history from the transaction pool index only.
void transaction_pool_index::fetch_index_history(
    const payment_address& address, query_handler handler)
{
    dispatch_.ordered(
        std::bind(&transaction_pool_index::do_fetch,
            this, address, handler));
}

template <typename InfoList, typename EntryMultimap>
InfoList to_info_list(const payment_address& address, EntryMultimap& map)
{
    InfoList info;
    auto pair = map.equal_range(address);
    for (auto it = pair.first; it != pair.second; ++it)
        info.push_back(it->second);

    return info;
}

void transaction_pool_index::do_fetch(const payment_address& address,
    query_handler handler)
{
    // This is the end of the fetch_index_history sequence.
    handler(error::success,
        to_info_list<spend_info::list>(address, spends_map_),
        to_info_list<output_info::list>(address, outputs_map_));
}

// ----------------------------------------------------------------------------
// Static helpers

bool transaction_pool_index::exists(block_chain::history& history,
    const spend_info& spend)
{
    // Transactions may exist in the memory pool and in the blockchain,
    // although this circumstance should not persist.
    for (const auto& row: history)
        if (row.kind == block_chain::point_kind::spend &&
            row.point == spend.point)
            return true;

    return false;
}

bool transaction_pool_index::exists(block_chain::history& history,
    const output_info& output)
{
    // Transactions may exist in the memory pool and in the blockchain,
    // although this circumstance should not persist.
    for (const auto& row: history)
        if (row.kind == block_chain::point_kind::output &&
            row.point == output.point)
            return true;

    return false;
}

void transaction_pool_index::add(block_chain::history& history,
    const spend_info& spend)
{
    const auto row = block_chain::history_row
    {
        block_chain::point_kind::spend,
        spend.point,
        genesis_height,
        { block_chain::spend_checksum(spend.previous_output) }
    };

    history.emplace_back(row);
}

void transaction_pool_index::add(block_chain::history& history,
    const output_info& output)
{
    const auto row = block_chain::history_row
    {
        block_chain::point_kind::output,
        output.point,
        genesis_height,
        { output.value }
    };

    history.emplace_back(row);
}

void transaction_pool_index::add(block_chain::history& history,
    const spend_info::list& spends)
{
    for (const auto& spend: spends)
        if (!exists(history, spend))
            add(history, spend);
}

void transaction_pool_index::add(block_chain::history& history,
    const output_info::list& outputs)
{
    for (const auto& output: outputs)
        if (!exists(history, output))
            add(history, output);
}

} // namespace blockchain
} // namespace libbitcoin
