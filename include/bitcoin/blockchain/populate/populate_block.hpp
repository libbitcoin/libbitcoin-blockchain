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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_BLOCK_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_BLOCK_HPP

#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/populate/populate_base.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is NOT thread safe.
class BCB_API populate_block
  : public populate_base
{
public:
    populate_block(dispatcher& dispatch, const fast_chain& chain);

    /// Populate validation state for the top block.
    void populate(branch::const_ptr branch, result_handler&& handler) const;

protected:
    typedef branch::const_ptr branch_ptr;

    void populate_coinbase(branch::const_ptr branch,
        block_const_ptr block) const;

    ////void populate_duplicate(branch_ptr branch,
    ////    const chain::transaction& tx) const;

    void populate_transactions(branch::const_ptr branch, size_t bucket,
        size_t buckets, result_handler handler) const;

    void populate_prevout(branch_ptr branch,
        const chain::output_point& outpoint) const;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
