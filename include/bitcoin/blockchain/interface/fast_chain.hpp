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
#ifndef LIBBITCOIN_BLOCKCHAIN_FAST_CHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_FAST_CHAIN_HPP

#include <cstddef>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>

namespace libbitcoin {
namespace blockchain {

/// A low level interface for encapsulation of the blockchain database.
/// Caller must ensure the database is not otherwise in use during these calls.
/// Implementations are NOT expected to be thread safe with the exception
/// that the import method may itself be called concurrently.
class BCB_API fast_chain
{
public:
    typedef handle0 complete_handler;

    // Readers.
    // ------------------------------------------------------------------------

    /// Get the set of block gaps in the chain.
    virtual bool get_gaps(
        database::block_database::heights& out_gaps) const = 0;

    /// Get a determination of whether the block hash exists in the store.
    virtual bool get_block_exists(const hash_digest& block_hash) const = 0;

    /// Get the hash of the block if it exists.
    virtual bool get_block_hash(hash_digest& out_hash,
        size_t height) const = 0;

    /// Get the work of the branch starting at the given height.
    virtual bool get_branch_work(uint256_t& out_work,
        const uint256_t& maximum, size_t from_height) const = 0;

    /// Get the header of the block at the given height.
    virtual bool get_header(chain::header& out_header,
        size_t height) const = 0;

    /// Get the height of the block with the given hash.
    virtual bool get_height(size_t& out_height,
        const hash_digest& block_hash) const = 0;

    /// Get the bits of the block with the given height.
    virtual bool get_bits(uint32_t& out_bits, const size_t& height) const = 0;

    /// Get the timestamp of the block with the given height.
    virtual bool get_timestamp(uint32_t& out_timestamp,
        const size_t& height) const = 0;

    /// Get the version of the block with the given height.
    virtual bool get_version(uint32_t& out_version,
        const size_t& height) const = 0;

    /// Get height of latest block.
    virtual bool get_last_height(size_t& out_height) const = 0;

    /// Get the output that is referenced by the outpoint.
    virtual bool get_output(chain::output& out_output, size_t& out_height,
        bool& out_coinbase, const chain::output_point& outpoint,
        size_t branch_height) const = 0;

    /// Determine if an unspent transaction exists with the given hash.
    virtual bool get_is_unspent_transaction(const hash_digest& hash,
        size_t branch_height) const = 0;

    /// Get the transaction of the given hash and its block height.
    virtual transaction_ptr get_transaction(size_t& out_block_height,
        const hash_digest& hash) const = 0;

    // Synchronous writers.
    // ------------------------------------------------------------------------ 

    /// Set the flush lock scope (for use only with insert).
    virtual bool begin_writes() = 0;

    /// Reset the flush lock scope (for use only with insert).
    virtual bool end_writes() = 0;

    /// Insert a block to the blockchain, height is checked for existence.
    virtual bool insert(block_const_ptr block, size_t height, bool flush) = 0;

    // Asynchronous writers.
    // ------------------------------------------------------------------------ 

    /// Swap incoming and outgoing blocks, height is validated.
    virtual void reorganize(branch::const_ptr branch,
        block_const_ptr_list_ptr outgoing_blocks, bool flush,
        dispatcher& dispatch, complete_handler handler) = 0;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
