/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/blockchain/populate/populate_header.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;
using namespace bc::system::chain;
using namespace bc::system::machine;

#define NAME "populate_header"

populate_header::populate_header(dispatcher& dispatch, const fast_chain& chain)
  : populate_base(dispatch, chain)
{
}

void populate_header::populate(header_branch::ptr branch,
    result_handler&& handler) const
{
    // The header could not be connected to the header index.
    if (!set_branch_state(branch))
    {
        handler(error::orphan_block);
        return;
    }

    const auto& header = *branch->top();

    // This is only needed for duplicate or stored error.
    fast_chain_.populate_header(header);

    // TODO: if it's errored, that's simple - the header fails validation.
    // TODO: if it's orphaned, then need to continue and mark as candidate.
    // TODO: if it's a candidate it's a duplicate.
    // TODO: but what if it's confirmed???
    if (header.metadata.exists)
    {
        // If there is an existing full block validation error return it.
        if (header.metadata.error)
        {
            handler(header.metadata.error);
            return;
        }

        handler(error::duplicate_block);
        return;
    }

    // HACK: allows header collection to carry median_time_past to store.
    // HACK: this is not done in store as it does not understand chain state.
    // HACK: so population is simulated here by forwarding from chain state.
    const auto median_time_past = header.metadata.state->median_time_past();
    header.metadata.median_time_past = median_time_past;

    // This header is not found in the store.
    handler(error::success);
}

// private
bool populate_header::set_branch_state(header_branch::ptr branch) const
{
    BITCOIN_ASSERT(!branch->empty());
    const auto branch_top = branch->top();
    auto& top_metadata = branch_top->metadata;

    // Promote chain state from top->parent to top.
    // TODO: assert that this always succeeds if the branch is not solo.
    top_metadata.state = fast_chain_.promote_state(branch);

    if (!top_metadata.state && branch->size() > 1u)
        return false;

    config::checkpoint chain_top;
    if (!fast_chain_.get_top(chain_top, true))
        return false;

    // If set this implies a pool ancestor (and height already set).
    if (top_metadata.state)
        return true;

    // This implies a solo branch, so promote the top or fork point.
    BITCOIN_ASSERT(branch->size() == 1u);

    // This grounds the branch at the top of candidate chain using state cache.
    if (branch_top->previous_block_hash() == chain_top.hash())
    {
        branch->set_fork_height(chain_top.height());
        const auto top_state = fast_chain_.top_candidate_state();
        top_metadata.state = fast_chain_.promote_state(*branch_top, top_state);
        BITCOIN_ASSERT(top_metadata.state);
        return true;
    }

    size_t fork_height;
    chain::header fork_header;
    const auto fork_hash = branch->fork_hash();

    // The grounding candidate may not be valid, but eventually is handled.
    // This grounds the branch at any point in candidate chain using new state.
    // This is the only case in which the chain is hit for state after startup.
    if (fast_chain_.get_header(fork_header, fork_height, fork_hash, true))
    {
        branch->set_fork_height(fork_height);

        // Query and create chain state for fork point (since not top).
        const auto fork_state = fast_chain_.chain_state(fork_header, fork_height);
        BITCOIN_ASSERT(fork_state);

        // Promote chain state from fork point to the only branch header.
        top_metadata.state = fast_chain_.promote_state(*branch_top, fork_state);
        BITCOIN_ASSERT(top_metadata.state);
        return true;
    }

    // Parent hash not found in header index.
    return false;
}

} // namespace blockchain
} // namespace libbitcoin
