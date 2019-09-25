/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/populate/populate_base.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API populate_block
  : public populate_base
{
public:
    populate_block(system::dispatcher& dispatch, const fast_chain& chain,
        bool catalog, bool neutrino_filter);

    /// Populate validation state for the the next block.
    void populate(system::block_const_ptr block,
        result_handler&& handler) const;

protected:
    void populate_coinbase(system::block_const_ptr block,
        size_t fork_height) const;
    void populate_non_coinbase(system::block_const_ptr block,
        size_t fork_height, bool use_txs, result_handler handler) const;
    void populate_transactions(system::block_const_ptr block,
        size_t fork_height, size_t bucket, size_t buckets, bool populate_txs,
        result_handler handler) const;
    void populate_neutrino_filter(system::block_const_ptr block) const;

private:
    const bool catalog_;
    const bool neutrino_filter_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
