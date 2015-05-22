/**
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
#include "validate_block_impl.hpp"

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
    namespace chain {

validate_block_impl::validate_block_impl(
    db_interface& database, int fork_index,
    const block_detail_list& orphan_chain, int orphan_index,
    size_t height, const block_type& current_block)
  : validate_block(height, current_block), interface_(database),
    height_(height), fork_index_(fork_index),
    orphan_index_(orphan_index), orphan_chain_(orphan_chain)
{
}

block_header_type validate_block_impl::fetch_block(size_t fetch_height)
{
    if (fetch_height > fork_index_)
    {
        size_t fetch_index = fetch_height - fork_index_ - 1;
        BITCOIN_ASSERT(fetch_index <= orphan_index_);
        BITCOIN_ASSERT(orphan_index_ < orphan_chain_.size());
        return orphan_chain_[fetch_index]->actual().header;
    }
    // We only really need the bits and timestamp fields.
    auto result = interface_.blocks.get(fetch_height);
    BITCOIN_ASSERT(result);
    return result.header();
}

uint32_t validate_block_impl::previous_block_bits()
{
    // Read block d - 1 and return bits
    return fetch_block(height_ - 1).bits;
}

uint64_t validate_block_impl::actual_timespan(const uint64_t interval)
{
    // Warning: conversion from 'uint64_t' to 'uint32_t', 
    // possible loss of data in fetch_block parameterization.
    BITCOIN_ASSERT(interval <= UINT32_MAX);

    // height - interval and height - 1, return time difference
    return fetch_block(height_ - 1).timestamp -
        fetch_block(height_ - (uint32_t)interval).timestamp;
}

uint64_t validate_block_impl::median_time_past()
{
    // read last 11 block times into array and select median value
    std::vector<uint64_t> times;
    int first = static_cast<int>(height_) - 1, last = first - 10;
    for (int i = first; i >= 0 && i >= last; --i)
        times.push_back(fetch_block(i).timestamp);
    BITCOIN_ASSERT(
        (height_ < 11 && times.size() == height_) || times.size() == 11);
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

bool tx_after_fork(size_t tx_height, size_t fork_index)
{
    if (tx_height <= fork_index)
        return false;
    return true;
}

bool validate_block_impl::transaction_exists(const hash_digest& tx_hash)
{
    auto result = interface_.transactions.get(tx_hash);
    if (!result)
        return false;
    return !tx_after_fork(result.height(), fork_index_);
}

bool validate_block_impl::is_output_spent(
    const output_point& outpoint)
{
    auto result = interface_.spends.get(outpoint);
    if (!result)
        return false;
    // Lookup block height. Is the spend after the fork point?
    return transaction_exists(result.hash());
}

bool validate_block_impl::fetch_transaction(transaction_type& tx,
    size_t& tx_height, const hash_digest& tx_hash)
{
    auto result = interface_.transactions.get(tx_hash);
    if (!result || tx_after_fork(result.height(), fork_index_))
    {
        return fetch_orphan_transaction(tx, tx_height, tx_hash);
    }
    tx = result.transaction();
    tx_height = result.height();
    return true;
}

bool validate_block_impl::fetch_orphan_transaction(
    transaction_type& tx, size_t& tx_height, const hash_digest& tx_hash)
{
    for (size_t orphan_iter = 0; orphan_iter <= orphan_index_; ++orphan_iter)
    {
        const block_type& orphan_block =
            orphan_chain_[orphan_iter]->actual();
        for (const transaction_type& orphan_tx: orphan_block.transactions)
        {
            if (hash_transaction(orphan_tx) == tx_hash)
            {
                tx = orphan_tx;
                tx_height = fork_index_ + orphan_iter + 1;
                return true;
            }
        }
    }
    return false;
}

bool validate_block_impl::is_output_spent(
    const output_point& previous_output,
    size_t index_in_parent, size_t input_index)
{
    // Search for double spends
    //   This must be done in both chain AND orphan
    // Searching chain when this tx is an orphan is redundant but
    // it does not happen enough to care
    if (is_output_spent(previous_output))
        return true;
    else if (orphan_is_spent(previous_output, index_in_parent, input_index))
        return true;
    return false;
}

bool validate_block_impl::orphan_is_spent(
    const output_point& previous_output,
    size_t skip_tx, size_t skip_input)
{
    // TODO factor this to look nicer
    for (size_t orphan_iter = 0; orphan_iter <= orphan_index_; ++orphan_iter)
    {
        const block_type& orphan_block =
            orphan_chain_[orphan_iter]->actual();
        // Skip coinbase
        BITCOIN_ASSERT(orphan_block.transactions.size() >= 1);
        BITCOIN_ASSERT(is_coinbase(orphan_block.transactions[0]));
        for (size_t tx_index = 0; tx_index < orphan_block.transactions.size();
            ++tx_index)
        {
            const transaction_type& orphan_tx =
                orphan_block.transactions[tx_index];
            for (size_t input_index = 0; input_index < orphan_tx.inputs.size();
                ++input_index)
            {
                const transaction_input_type& orphan_input =
                    orphan_tx.inputs[input_index];
                if (orphan_iter == orphan_index_ && tx_index == skip_tx &&
                    input_index == skip_input)
                {
                    continue;
                }
                else if (orphan_input.previous_output == previous_output)
                    return true;
            }
        }
    }
    return false;
}

    } // namespace chain
} // namespace libbitcoin
