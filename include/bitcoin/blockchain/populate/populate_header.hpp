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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_HEADER_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_HEADER_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/populate/populate_base.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API populate_header
  : public populate_base
{
public:
    populate_header(dispatcher& dispatch, const fast_chain& chain,
        const bc::settings& bitcoin_settings);

    /// Populate validation state for the top indexed block.
    void populate(header_branch::ptr branch, result_handler&& handler) const;

private:
    bool set_branch_state(header_branch::ptr branch) const;

    const bc::settings& bitcoin_settings_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
