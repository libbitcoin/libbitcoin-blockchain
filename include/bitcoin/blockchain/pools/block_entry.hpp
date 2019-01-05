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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_ENTRY_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_ENTRY_HPP

#include <cstddef>
#include <boost/functional/hash_fwd.hpp>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
class BCB_API block_entry
{
public:
    /// Construct an entry for the pool.
    block_entry(system::block_const_ptr block, size_t height);

    /// Use this construction only as a search key.
    block_entry(const system::hash_digest& hash);

    /// The block that the entry contains.
    system::block_const_ptr block() const;

    /// The height of the block the entry contains.
    size_t height() const;

    /// The hash table entry identity.
    const system::hash_digest& hash() const;

    /// Operators.
    bool operator==(const block_entry& other) const;

private:
    // These are non-const to allow for default copy construction.
    size_t height_;
    system::hash_digest hash_;
    system::block_const_ptr block_;
};

} // namespace blockchain
} // namespace libbitcoin

// Standard (boost) hash.
//-----------------------------------------------------------------------------

namespace boost
{

// Extend boost namespace with our block_entry hash function.
template <>
struct hash<bc::blockchain::block_entry>
{
    size_t operator()(const bc::blockchain::block_entry& entry) const
    {
        return boost::hash<bc::system::hash_digest>()(entry.hash());
    }
};

} // namespace boost

#endif
