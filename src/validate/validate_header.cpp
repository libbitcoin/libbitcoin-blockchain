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
#include <bitcoin/blockchain/validate/validate_header.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;
using namespace std::placeholders;

#define NAME "validate_header"

validate_header::validate_header(dispatcher& dispatch, const fast_chain& chain,
    const settings& settings, const bc::settings& bitcoin_settings)
  : stopped_(true),
    retarget_(settings.retarget),
    header_populator_(dispatch, chain, bitcoin_settings)
{
}

// Properties.
//-----------------------------------------------------------------------------

bool validate_header::stopped() const
{
    return stopped_;
}

// Start/stop sequences.
//-----------------------------------------------------------------------------

void validate_header::start()
{
    stopped_ = false;
}

void validate_header::stop()
{
    stopped_ = true;
}

// Check.
//-----------------------------------------------------------------------------
// These checks are context free.

code validate_header::check(header_const_ptr header) const
{
    // Run context free checks, even if under checkpoint or milestone.
    return header->check(retarget_);
}

// Accept sequence.
//-----------------------------------------------------------------------------
// These checks require chain state (net height and enabled forks).

void validate_header::accept(header_branch::ptr branch,
    result_handler handler) const
{
    // Populate header state for the top header (others are valid).
    header_populator_.populate(branch,
        std::bind(&validate_header::handle_populated,
            this, _1, branch, handler));
}

void validate_header::handle_populated(const code& ec,
    header_branch::ptr branch, result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        handler(ec);
        return;
    }

    const auto& header = *branch->top();

    // Skip validation if full block was validated (is valid at this point).
    if (header.metadata.validated)
    {
        handler(error::success);
        return;
    }

    BITCOIN_ASSERT(header.metadata.state);

    // Run contextual header checks.
    handler(header.accept());
}

} // namespace blockchain
} // namespace libbitcoin
