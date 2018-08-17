/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;

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
    fast_chain_.populate_header(header);

    // TODO: ensure there is no need to set header state or index here.
    if (header.metadata.exists)
    {
        handler(error::duplicate_block);
        return;
    }

    // HACK: allows header collection to carry median_time_past to store.
    header.metadata.median_time_past = header.metadata.state->
        median_time_past();

    // If there is an existing full block validation error return it.
    handler(header.metadata.error);
}

// private
bool populate_header::set_branch_state(header_branch::ptr branch) const
{
    BITCOIN_ASSERT(!branch->empty());
    const auto branch_top = branch->top();
    auto& metadata = branch_top->metadata;
    metadata.state = fast_chain_.promote_state(branch);

    // If set this implies a pool ancestor (and height already set).
    if (metadata.state)
    {
        BITCOIN_ASSERT(branch->height() != max_size_t);
        return true;
    }

    config::checkpoint chain_top;
    const auto& parent = branch_top->previous_block_hash();

    // This grounds the branch at the top of candidate chain using state cache.
    if (fast_chain_.get_top(chain_top, true) && parent == chain_top.hash())
    {
        branch->set_height(chain_top.height());
        const auto top_state = fast_chain_.top_candidate_state();
        metadata.state = fast_chain_.promote_state(*branch_top, top_state);
        BITCOIN_ASSERT(metadata.state);
        return true;
    }

    size_t fork_height;
    chain::header fork_header;
    const auto fork_hash = branch->hash();

    // The grounding candidate may not be valid, but eventually is handled.
    // This grounds the branch at any point in candidate chain using new state.
    // This is the only case in which the chain is hit for state after startup.
    if (fast_chain_.get_header(fork_header, fork_height, fork_hash, true))
    {
        branch->set_height(fork_height);
        metadata.state = fast_chain_.chain_state(fork_header, fork_height);
        BITCOIN_ASSERT(metadata.state);
        return true;
    }

    // Parent hash not found in header index.
    return false;
}

} // namespace blockchain
} // namespace libbitcoin
