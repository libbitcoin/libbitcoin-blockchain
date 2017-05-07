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
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_input.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;
using namespace std::placeholders;

#define NAME "validate_header"

// Database access is limited to: populator:
// block: { bits, version, timestamp }

validate_header::validate_header(dispatcher& dispatch,
    const fast_chain& chain, const settings& settings)
  : stopped_(true),
    fast_chain_(chain),
    header_populator_(dispatch, chain)
{
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

void validate_header::check(header_const_ptr header,
    result_handler handler) const
{
    // Run context free checks.
    handler(header->check());
}

// Accept sequence.
//-----------------------------------------------------------------------------
// These checks require chain state (net height and enabled forks).

void validate_header::accept(header_const_ptr header,
    result_handler handler) const
{
    header_populator_.populate(header,
        std::bind(&validate_header::handle_populated,
            this, _1, header, handler));
}

void validate_header::handle_populated(const code& ec, header_const_ptr header,
    result_handler handler) const
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

    // Run contextual header checks.
    handler(header->accept());
}

} // namespace blockchain
} // namespace libbitcoin
