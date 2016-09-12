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
#include <bitcoin/blockchain/validate_transaction.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace chain;
using namespace std::placeholders;

// Maximum transaction size is set to max block size (1,000,000).
static constexpr uint32_t max_transaction_size = 1000000;

// Maximum signature operations is set to block limit (20000).
static constexpr uint32_t max_block_script_sigops = max_transaction_size / 50;

validate_transaction::validate_transaction(block_chain& chain,
    transaction_ptr tx, const transaction_pool& pool,
    dispatcher& dispatch)
  : blockchain_(chain),
    tx_(tx),
    pool_(pool),
    dispatch_(dispatch),
    tx_hash_(tx->hash())
{
}

validate_transaction::validate_transaction(block_chain& chain,
    const chain::transaction& tx, const transaction_pool& pool,
    dispatcher& dispatch)
  : validate_transaction(chain,
        std::make_shared<message::transaction_message>(tx), pool, dispatch)
{
}

// mempool check
void validate_transaction::start(validate_handler handler)
{
    handle_validate_ = handler;
    const auto ec = check_for_mempool();

    if (ec)
    {
        handle_validate_(ec, tx_, {});
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: change to fetch_unspent_transaction, spent dups ok (BIP30).
    ///////////////////////////////////////////////////////////////////////////
    // Check for duplicates in the blockchain.
    blockchain_.fetch_transaction(tx_hash_,
        dispatch_.unordered_delegate(
            &validate_transaction::handle_duplicate_check,
                shared_from_this(), _1));
}

// mempool check
code validate_transaction::check_for_mempool() const
{
    if (tx_->is_coinbase())
        return error::coinbase_transaction;

    // Basic checks that are independent of the mempool.
    const auto ec = check_transaction(*tx_);

    if (ec)
        return ec;

    // This wasn't done in version 2.x.
    ////// check_block counts sigops once for all transactions so check_transaction
    ////// excludes this check as optimization and we handle here independently.
    ////if (tx_->signature_operations(false) > max_block_script_sigops)
    ////    return error::too_many_sigs;

    return pool_.is_in_pool(tx_hash_) ? error::duplicate : error::success;
}

// mempool check
void validate_transaction::handle_duplicate_check(const code& ec)
{
    if (ec != error::not_found)
    {
        ///////////////////////////////////////////////////////////////////////
        // BUGBUG: overly restrictive, spent dups ok (BIP30).
        ///////////////////////////////////////////////////////////////////////
        handle_validate_(error::duplicate, tx_, {});
        return;
    }

    // TODO: we may want to allow spent-in-pool (RBF).
    if (pool_.is_spent_in_pool(tx_))
    {
        handle_validate_(error::double_spend, tx_, {});
        return;
    }

    // Check inputs, we already know it is not a coinbase tx.
    blockchain_.fetch_last_height(
        dispatch_.unordered_delegate(&validate_transaction::set_last_height,
            shared_from_this(), _1, _2));
}

// mempool check
void validate_transaction::set_last_height(const code& ec,
    size_t last_height)
{
    if (ec)
    {
        handle_validate_(ec, tx_, {});
        return;
    }

    // Used for checking coinbase maturity
    last_block_height_ = last_height;
    current_input_ = 0;
    value_in_ = 0;

    // Begin looping through the inputs, fetching each previous tx.
    if (!tx_->inputs.empty())
        next_previous_transaction();
}

// mempool check
void validate_transaction::next_previous_transaction()
{
    BITCOIN_ASSERT(current_input_ < tx_->inputs.size());

    // First we fetch the parent block height for a transaction.
    // Needed for checking the coinbase maturity.
    blockchain_.fetch_transaction_index(
        tx_->inputs[current_input_].previous_output.hash,
            dispatch_.unordered_delegate(
                &validate_transaction::previous_tx_index,
                    shared_from_this(), _1, _2));
}

// mempool check
void validate_transaction::previous_tx_index(const code& ec,
    size_t parent_height)
{
    if (ec)
    {
        search_pool_previous_tx();
        return;
    }

    const auto& prev_tx_hash = tx_->inputs[current_input_].previous_output.hash;

    // Now fetch actual previous transaction body.
    blockchain_.fetch_transaction(prev_tx_hash,
        dispatch_.unordered_delegate(&validate_transaction::handle_previous_tx,
            shared_from_this(), _1, _2, parent_height));
}

// mempool check
void validate_transaction::search_pool_previous_tx()
{
    transaction previous_tx;
    const auto& current_input = tx_->inputs[current_input_];

    if (!pool_.find(previous_tx, current_input.previous_output.hash))
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error::input_not_found, tx_, list);
        return;
    }

    BITCOIN_ASSERT(!previous_tx.is_coinbase());

    // parent_height ignored here as mempool transactions cannot be coinbase.
    static constexpr size_t parent_height = 0;
    handle_previous_tx(error::success, previous_tx, parent_height);
    unconfirmed_.push_back(current_input_);
}

// mempool check
void validate_transaction::handle_previous_tx(const code& ec,
    const transaction& previous_tx, size_t parent_height)
{
    if (ec)
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error::input_not_found, tx_, list);
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // HACK: this assumes that the mempool is operating at min block version 4.
    ///////////////////////////////////////////////////////////////////////////

    const auto error_code = check_input(*tx_, current_input_, previous_tx,
        parent_height, last_block_height_, value_in_,
        script_context::all_enabled);

    if (error_code)
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error_code, tx_, list);
        return;
    }

    const auto& output = tx_->inputs[current_input_].previous_output;

    // Search for double spends in blockchain store...
    blockchain_.fetch_spend(output,
        dispatch_.unordered_delegate(&validate_transaction::handle_spend,
            shared_from_this(), _1, _2));
}

// mempool check
void validate_transaction::handle_spend(const code& ec,
    const chain::input_point&)
{
    if (ec != error::not_found)
    {
        handle_validate_(error::double_spend, tx_, {});
        return;
    }

    ++current_input_;

    // current_input_ becomes invalid on last pass.
    if (current_input_ < tx_->inputs.size())
    {
        next_previous_transaction();
        return;
    }

    // End of mempool checks loop.

    if (tx_->total_output_value() > value_in_)
    {
        handle_validate_(error::spend_exceeds_value, tx_, {});
        return;
    }

    // Who cares?
    // Fuck the police
    // Every tx equal!
    handle_validate_(error::success, tx_, unconfirmed_);
}

// common inexpensive checks
code validate_transaction::check_transaction(const transaction& tx)
{
    if (tx.inputs.empty() || tx.outputs.empty())
        return error::empty_transaction;

    if (tx.serialized_size() > max_transaction_size)
        return error::size_limits;

    if (tx.total_output_value() > max_money())
        return error::output_value_overflow;

    if (tx.is_invalid_coinbase())
        return error::invalid_coinbase_script_size;

    if (tx.is_invalid_non_coinbase())
        return error::previous_output_null;

    return error::success;
}

// common checks
// Validate script consensus conformance based on flags provided.
code validate_transaction::check_consensus(const script& prevout_script,
    const transaction& current_tx, size_t input_index, uint32_t flags)
{
    BITCOIN_ASSERT(input_index <= max_uint32);
    BITCOIN_ASSERT(input_index < current_tx.inputs.size());
    const auto input_index32 = static_cast<uint32_t>(input_index);

#ifdef WITH_CONSENSUS
    using namespace bc::consensus;
    const auto previous_output_script = prevout_script.to_data(false);
    data_chunk current_transaction = current_tx.to_data();

    // Convert native flags to libbitcoin-consensus flags.
    uint32_t consensus_flags = verify_flags_none;

    if ((flags & script_context::bip16_enabled) != 0)
        consensus_flags |= verify_flags_p2sh;

    if ((flags & script_context::bip65_enabled) != 0)
        consensus_flags |= verify_flags_checklocktimeverify;

    if ((flags & script_context::bip66_enabled) != 0)
        consensus_flags |= verify_flags_dersig;

    const auto result = verify_script(current_transaction.data(),
        current_transaction.size(), previous_output_script.data(),
        previous_output_script.size(), input_index32, consensus_flags);

    const auto valid = (result == verify_result::verify_result_eval_true);
#else
    const auto valid = script::verify(current_tx.inputs[input_index].script,
        prevout_script, current_tx, input_index32, flags);
#endif

    return valid ? error::success : error::validate_inputs_failed;
}

// common checks
code validate_transaction::check_input(const transaction& tx,
    size_t input_index, const transaction& previous_tx,
    size_t parent_height, size_t previous_height, uint64_t& value,
    uint32_t flags)
{
    const auto& input = tx.inputs[input_index];
    const auto& previous_output = input.previous_output;

    if (previous_output.index >= previous_tx.outputs.size())
        return error::input_not_found;

    const auto& previous_tx_out = previous_tx.outputs[previous_output.index];
    const auto output_value = previous_tx_out.value;

    BITCOIN_ASSERT(previous_height <= parent_height);

    if (previous_tx.is_coinbase() &&
        (previous_height - parent_height < coinbase_maturity))
        return error::coinbase_maturity;

    if (output_value > max_money())
        return error::spend_exceeds_value;

    if (value > max_uint64 - output_value)
        return error::spend_exceeds_value;

    // This function accumulates and value_in as a side effect.
    // This is an optimization due to the cost if previous tx lookup.
    value += output_value;

    if (value > max_money())
        return error::output_value_overflow;

    return check_consensus(previous_tx_out.script, tx, input_index, flags);
}

} // namespace blockchain
} // namespace libbitcoin
