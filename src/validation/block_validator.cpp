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
#include <bitcoin/blockchain/validation/block_validator.hpp>

#include <algorithm>
#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/simple_chain.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;

// The height parameter is the new block (top + 1).
block_validator::block_validator(size_t fork_height,
    const block_const_ptr_list& orphan_chain, size_t orphan_index,
    const block_const_ptr block, size_t height, bool testnet,
    const checkpoints& checks, const simple_chain& chain)
  : height_(height),
    fork_height_(fork_height),
    orphan_index_(orphan_index),
    orphan_chain_(orphan_chain),
    chain_(chain)
{
    // Next block can never be genesis.
    BITCOIN_ASSERT(height_ != 0);

    // Orphan index can never euqual or exceed chain size.
    BITCOIN_ASSERT(orphan_index_ < orphan_chain_.size());
}

// TODO: populate required data into chain state and move logic to block.
// Chain state will need to be populated using height and current fork state.
// fork_state: orphan_chain_/orphan_index_/fork_height_
// But once populated all validation will require only chain_state for context.

// get_block_versions
// ----------------------------------------------------------------------------

// Requires [top 100|1000].versions.
bool block_validator::get_block_versions(versions& out_history, size_t height,
    size_t maximum) const
{
    // 1000 previous versions maximum sample.
    // 950 previous versions minimum required for enforcement.
    // 750 previous versions minimum required for activation.
    const auto size = std::min(height, maximum);
    out_history.reserve(size);
    out_history.clear();
    uint32_t version;

    // Read block (top) through (top - 99|999) and return versions.
    for (size_t index = 0; index < size; ++index)
    {
        // The index is guaranteed to be less than height (and maximum).
        if (!fetch_version(version, height - index - 1))
            return false;

        // TODO: review BIP9 (version bits).
        // Some blocks have high versions, see block #390777.
        static const auto maximum = static_cast<uint32_t>(max_uint8);
        const auto normal = std::min(version, maximum);
        out_history.push_back(static_cast<uint8_t>(normal));
    }

    return true;
}

// median_time_past
// ----------------------------------------------------------------------------

// Requires [top 11].timestamp.
bool block_validator::median_time_past(uint64_t out_time_past,
    size_t height) const
{
    // Read last 11 (or height if height < 11) block times into array.
    const auto count = std::min(height, median_time_past_blocks);
    std::vector<uint64_t> times(count);
    uint32_t timestamp;

    for (size_t index = 0; index < count; ++index)
    {
        // The index is guaranteed to be less than height (and mtpb).
        if (!fetch_timestamp(timestamp, height - index - 1))
            return false;

        times[index] = timestamp;
    }

    // Sort and select middle (median) value from the array.
    std::sort(times.begin(), times.end());
    out_time_past = times.empty() ? 0 : times[times.size() / 2];
    return true;
}



// work_required
//-----------------------------------------------------------------------------

// Requires [top].bits (mainnet).
// Requires [top].timestamp AND [top - 2015].timestamp.
bool block_validator::work_required(uint32_t out_work, size_t height,
    uint32_t timestamp, bool is_testnet) const
{
    if (height == 0)
        return max_work_bits;

    if (block::is_retarget_height(height))
    {
        // Get the total time spanning the last 2016 blocks.
        uint64_t timespan;
        if (!retarget_timespan(timespan, height))
            return false;

        // Get the preceeding block's bits.
        uint32_t bits;
        if (!fetch_bits(bits, height - 1))
            return false;

        out_work = block::work_required(timespan, bits);
        return true;
    }

    return is_testnet ? work_required_testnet(out_work, height, timestamp) :
        fetch_bits(out_work, height - 1);
}

// Requires [top].timestamp and [top].bits through up to [top - 2014].bits.
bool block_validator::work_required_testnet(uint32_t out_work, size_t height,
    uint32_t timestamp) const
{
    uint32_t time;

    if (!fetch_timestamp(time, safe_subtract(height, size_t(1))))
        return false;

    const auto max_time_gap = time + 2 * target_spacing_seconds;

    if (timestamp > max_time_gap)
    {
        out_work = max_work_bits;
        return true;
    }

    auto found = false;
    uint32_t previous_bits;
    auto previous_height = height;

    // Loop backwards until we find a retarget point, or we find a block which
    // does not have max_bits (is not special). Zero is a retarget height.
    while (!found)
    {
        // previous_height is guarded by first subtract and is_retarget_height.
        if (!fetch_bits(previous_bits, --previous_height))
            return false;

        found = (previous_bits != max_work_bits) ||
            block::is_retarget_height(previous_height);
    }

    out_work = previous_bits;
    return true;
}

// Requires [top].timestamp AND [top - 2015].timestamp.
bool block_validator::retarget_timespan(uint64_t& out_timespan,
    size_t height) const
{
    uint32_t time_hi;
    uint32_t time_lo;

    // Zero height is precluded and first retarget height above zero is 2016.
    if (!fetch_timestamp(time_hi, safe_subtract(height, size_t(1))) ||
        !fetch_timestamp(time_lo, safe_subtract(height, retargeting_interval)))
        return false;

    out_timespan = time_hi - time_lo;
    return true;
}

// TODO: move hybrid queries to blockchain.
//-----------------------------------------------------------------------------

bool block_validator::fetch_bits(uint32_t& out_bits, size_t fetch_height) const
{
    if (fetch_height <= fork_height_)
        return chain_.get_bits(out_bits, fetch_height);

    const auto fetch_index = fetch_height - fork_height_ - 1;

    if (fetch_index > orphan_index_)
        return false;

    out_bits = orphan_chain_[fetch_index]->header.bits;
    return true;
}

bool block_validator::fetch_timestamp(uint32_t& out_timestamp,
    size_t fetch_height) const
{
    if (fetch_height <= fork_height_)
        return chain_.get_timestamp(out_timestamp, fetch_height);

    const auto fetch_index = fetch_height - fork_height_ - 1;

    if (fetch_index > orphan_index_)
        return false;

    out_timestamp = orphan_chain_[fetch_index]->header.timestamp;
    return true;
}

bool block_validator::fetch_version(uint32_t& out_version,
    size_t fetch_height) const
{
    if (fetch_height <= fork_height_)
        return chain_.get_version(out_version, fetch_height);

    const auto fetch_index = fetch_height - fork_height_ - 1;

    if (fetch_index > orphan_index_)
        return false;

    out_version = orphan_chain_[fetch_index]->header.version;
    return true;
}

// ----------------------------------------------------------------------------

transaction_ptr block_validator::fetch_transaction(size_t& tx_height,
    const hash_digest& tx_hash) const
{
    uint64_t out_tx_height;
    const auto tx = chain_.get_transaction(out_tx_height, tx_hash);

    if (tx && out_tx_height <= fork_height_)
    {
        BITCOIN_ASSERT(out_tx_height <= max_size_t);
        tx_height = static_cast<size_t>(out_tx_height);
        return tx;
    }

    return fetch_orphan_transaction(tx_height, tx_hash);
}

transaction_ptr block_validator::fetch_orphan_transaction(
    size_t& tx_height, const hash_digest& tx_hash) const
{
    for (size_t orphan = 0; orphan <= orphan_index_; ++orphan)
    {
        const auto& orphan_block = orphan_chain_[orphan];

        for (const auto& tx: orphan_block->transactions)
        {
            if (tx.hash() == tx_hash)
            {
                tx_height = fork_height_ + orphan + 1;
                //=============================================================
                // TRANSACTION COPY
                return std::make_shared<message::transaction_message>(tx);
                //=============================================================
            }
        }
    }

    return nullptr;
}

bool block_validator::is_output_spent(const output_point& outpoint) const
{
    uint64_t tx_height;
    hash_digest tx_hash;
    return
        chain_.get_transaction_hash(tx_hash, outpoint) &&
        chain_.get_transaction_height(tx_height, tx_hash) &&
        tx_height <= fork_height_;
}

bool block_validator::is_orphan_spent(const output_point& previous_output,
    const transaction& skip_tx, uint32_t skip_input_index) const
{
    // For each orphan...
    for (size_t orphan = 0; orphan <= orphan_index_; ++orphan)
    {
        const auto orphan_block = orphan_chain_[orphan];
        const auto& transactions = orphan_block->transactions;
        BITCOIN_ASSERT(!transactions.empty());
        BITCOIN_ASSERT(transactions.front().is_coinbase());

        // For each tx...
        for (size_t position = 0; position < transactions.size(); ++position)
        {
            const auto& orphan_tx = transactions[position];
            const auto inputs = orphan_tx.inputs.size();

            // For each input...
            for (size_t input_index = 0; input_index < inputs; ++input_index)
            {
                const auto& orphan_input = orphan_tx.inputs[input_index];

                // Avoid the orphan spend that we are testing.
                if (orphan == orphan_index_ &&
                    input_index == skip_input_index &&
                    skip_tx.hash() == orphan_tx.hash())
                    continue;

                if (orphan_input.previous_output == previous_output)
                    return true;
            }
        }
    }

    return false;
}

} // namespace blockchain
} // namespace libbitcoin
