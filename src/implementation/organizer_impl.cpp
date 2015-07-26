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
#include <bitcoin/blockchain/implementation/organizer_impl.hpp>

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoint.hpp>
#include <bitcoin/blockchain/implementation/validate_block_impl.hpp>

namespace libbitcoin {
namespace chain {

organizer_impl::organizer_impl(db_interface& database, orphans_pool& orphans,
    simple_chain& chain, reorganize_handler handler,
    const config::checkpoint::list& checks)
  : organizer(orphans, chain), interface_(database), handler_(handler),
    checkpoints_(checks)
{
    // Sort checkpoints by height so that top is sure to be properly obtained.
    checkpoint::sort(checkpoints_);
}

// For logging.
static size_t count_inputs(const block_type& block)
{
    size_t total_inputs = 0;
    for (const auto& tx : block.transactions)
        total_inputs += tx.inputs.size();

    return total_inputs;
}

bool organizer_impl::strict(size_t fork_point)
{
    return checkpoints_.empty() || fork_point > checkpoints_.back().height();
}

std::error_code organizer_impl::verify(size_t fork_point,
    const block_detail_list& orphan_chain, size_t orphan_index)
{
    BITCOIN_ASSERT(orphan_index < orphan_chain.size());
    const auto& current_block = orphan_chain[orphan_index]->actual();
    const size_t height = fork_point + orphan_index + 1;
    BITCOIN_ASSERT(height != 0);

    validate_block_impl validate(interface_, fork_point, orphan_chain,
        orphan_index, height, current_block, checkpoints_);

    // Checks that are independent of the chain.
    auto ec = validate.check_block();
    if (ec)
        return ec;

    // Checks that are dependent on height and preceding blocks.
    ec = validate.accept_block();
    if (ec)
        return ec;

    // Start strict validation if past last checkpoint.
    if (strict(fork_point))
    {
        const auto total_inputs = count_inputs(current_block);
        const auto total_transactions = current_block.transactions.size();

        log_info(LOG_BLOCKCHAIN)
            << "Block [" << height << "] verify ("
            << total_transactions << ") txs and ("
            << total_inputs << ") inputs";

        // Time this for logging.
        const auto timed = [&ec, &validate]()
        {
            // Checks that include input->output traversal.
            ec = validate.connect_block();
        };

        // Execute the timed validation.
        const auto elapsed = timer<std::chrono::milliseconds>::duration(timed);
        const auto ms_per_block = static_cast<float>(elapsed.count());
        const auto ms_per_input = ms_per_block / total_inputs;
        const auto secs_per_block = ms_per_block / 1000;
        const auto verified = ec ? "unverified" : "verified";

        log_info(LOG_BLOCKCHAIN)
            << "Block [" << height << "] " << verified << " in ("
            << secs_per_block << ") secs or ("
            << ms_per_input << ") ms/input";
    }

    return ec;
}

void organizer_impl::reorganize_occured(size_t fork_point,
    const blockchain::block_list& arrivals,
    const blockchain::block_list& replaced)
{
    handler_(bc::error::success, fork_point, arrivals, replaced);
}

} // namespace chain
} // namespace libbitcoin
