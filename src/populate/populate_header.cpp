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
#include <bitcoin/blockchain/populate/populate_base.hpp>

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
    // The header is already memory pooled (nothing to do).
    if (branch->empty())
    {
        handler(error::duplicate_block);
        return;
    }

    // TODO: why does the branch not already know its height?
    // The header could not be connected to the header index.
    if (!set_branch_height(branch))
    {
        handler(error::orphan_block);
        return;
    }

    const auto header = branch->top();
    fast_chain_.populate_header(*header);

    // TODO: return error::duplicate_block here.
    // There is a permanent previous validation error on the block.
    if (header->metadata.error != error::success)
    {
        handler(header->metadata.error);
        return;
    }

    // The header is already indexed (nothing to do).
    if (header->metadata.duplicate)
    {
        handler(error::duplicate_block);
        return;
    }

    // Always populate chain state so that we never hit the store to do so.
    const auto state = fast_chain_.chain_state(branch);
    header->metadata.state = state;

    if (!state)
    {
        handler(error::operation_failed);
        return;
    }

    handler(error::success);
}

bool populate_header::set_branch_height(header_branch::ptr branch) const
{
    size_t height;

    // Reject header if above fork point, it doesn't connect to indexed header.
    // This will cause rebuild of a confirmed block chain in the header pool
    // before it can overtake the header chain. Long branch competition can be
    // costly in terms of pool push/pop of headers in this scenario, however by
    // adding outgoing headers to the pool the cost is somewhat mitigated.
    // The greater cost of deep reorgs between stored blocks is updating state.
    const auto fork_height = fast_chain_.get_fork_point();

    // Get header index height of parent of the oldest branch block.
    if (!fast_chain_.get_block_height(height, branch->hash(), fork_height))
        return false;

    branch->set_height(height);
    return true;
}

} // namespace blockchain
} // namespace libbitcoin
