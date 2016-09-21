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
#include <bitcoin/blockchain/validation/validate_block.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::config;
using namespace std::placeholders;

#define NAME "validate_block"

validate_block::validate_block(threadpool& pool, bool testnet,
    const checkpoints& checkpoints, const simple_chain& chain)
  : chain_state_(testnet, checkpoints),
    history_(chain_state_.sample_size),
    stopped_(false),
    dispatch_(pool, NAME "_dispatch"),
    chain_(chain)
{
}

// Stop sequence (thread safe).
//-----------------------------------------------------------------------------

void validate_block::stop()
{
    stopped_.store(true);
}

bool validate_block::stopped() const
{
    return stopped_.load();
}

// Reset (not thread safe).
//-----------------------------------------------------------------------------

// Must call this to update chain state before calling accept or connect.
// There is no need to call a second time for connect after accept.
void validate_block::reset(size_t height, result_handler handler)
{
    //// get_block_versions(height, chain_state_.sample_size)
    //// median_time_past(time, height);
    //// work_required(work, height);
    //// work_required_testnet(work, height, candidate_block.timestamp);

    // This has a side effect on subsequent calls!
    chain_state_.set_context(height, history_);

    // TODO: modify query to return code on failure, including height mismatch.
    handler(error::success);
}

// Validation sequence (thread safe).
//-----------------------------------------------------------------------------

// These checks are context free (no reset).
void validate_block::check(block_const_ptr block, result_handler handler) const
{
    handler(block->check());
}

// These checks require height or other chain context (reset).
void validate_block::accept(block_const_ptr block,
    result_handler handler) const
{
    handler(block->accept(chain_state_));
}

// These checks require output traversal and validation (reset).
// We do not use block/tx.connect here because we want to fan out by input.
void validate_block::connect(block_const_ptr block,
    result_handler handler) const
{
    if (!chain_state_.use_full_validation())
    {
        log::info(LOG_BLOCKCHAIN)
            << "Block [" << chain_state_.next_height() << "] accepted ("
            << block->transactions.size() << ") txs and ("
            << block->total_inputs() << ") inputs under checkpoint.";
        handler(error::success);
        return;
    }

    const result_handler complete_handler =
        std::bind(&validate_block::handle_connect,
            this, _1, block, asio::steady_clock::now(), handler);

    const result_handler join_handler =
        synchronize(complete_handler, block->total_inputs(), NAME "_sync");

    // Skip the first transaction in order to avoid the coinbase.
    for (const auto& tx: block->transactions)
        for (uint32_t index = 0; index < tx.inputs.size(); ++index)
            ////dispatch_.concurrent(&validate_block::connect_input,
            ////    this, std::ref(tx), index, join_handler);
                connect_input(tx, index, join_handler);
}

void validate_block::connect_input(const transaction& tx, uint32_t input_index,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    handler(tx.connect_input(chain_state_, input_index));
}

void validate_block::handle_connect(const code& ec, block_const_ptr block,
    asio::time_point start_time, result_handler handler) const
{
    const auto delta = asio::steady_clock::now() - start_time;
    const auto elapsed = std::chrono::duration_cast<asio::milliseconds>(delta);
    const auto ms_per_block = static_cast<float>(elapsed.count());
    const auto ms_per_input = ms_per_block / block->total_inputs();
    const auto seconds_per_block = ms_per_block / 1000;
    const auto accepted = ec ? "aborted" : "accepted";

    log::info(LOG_BLOCKCHAIN)
        << "Block [" << chain_state_.next_height() << "] " << accepted << " ("
        << block->transactions.size() << ") txs in ("
        << seconds_per_block << ") secs or ("
        << ms_per_input << ") ms/input";

    handler(ec);
}

} // namespace blockchain
} // namespace libbitcoin

////bool validate_block::contains_duplicates(block_const_ptr block) const
////{
////    size_t height;
////    transaction other;
////    auto duplicate = false;
////    const auto& txs = block.transactions;
////
////    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
////    for (auto tx = txs.begin(); !duplicate && tx != txs.end(); ++tx)
////    {
////        duplicate = fetch_transaction(other, height, tx->hash());
////    }
////    //|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
////
////    return duplicate;
////}
////
// Lookup transaction and block height of the previous output.
////if (!fetch_transaction(previous_tx, previous_tx_height, previous_hash))
////    return error::input_not_found;