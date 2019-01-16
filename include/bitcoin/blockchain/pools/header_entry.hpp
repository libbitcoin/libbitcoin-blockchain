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
#ifndef LIBBITCOIN_BLOCKCHAIN_HEADER_ENTRY_HPP
#define LIBBITCOIN_BLOCKCHAIN_HEADER_ENTRY_HPP

#include <cstddef>
////#include <iostream>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <boost/functional/hash_fwd.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
class BCB_API header_entry
{
public:
    /// Construct an entry for the pool.
    header_entry(system::header_const_ptr header, size_t height);

    /// Use this construction only as a search key.
    header_entry(const system::hash_digest& hash);

    /// The header that the entry contains.
    system::header_const_ptr header() const;

    /// The height of the header the entry contains.
    size_t height() const;

    /// The hash table entry identity.
    const system::hash_digest& hash() const;

    /// The hash table entry's parent (preceding header) hash.
    const system::hash_digest& parent() const;

    /// The hash table entry's child (succeeding header) hashes.
    const system::hash_list& children() const;

    /// Add header to the list of children of this header.
    void add_child(system::header_const_ptr child) const;

    /// Operators.
    bool operator==(const header_entry& other) const;

    /////// Serializer for debugging (temporary).
    ////friend std::ostream& operator<<(std::ostream& out, const header_entry& of);

private:
    // These are non-const to allow for default copy construction.
    size_t height_;
    system::hash_digest hash_;
    system::header_const_ptr header_;

    // TODO: could save some bytes here by holding the pointer in place of the
    // hash. This would allow navigation to the hash saving 24 bytes per child.
    // Children do not pertain to entry hash, so must be mutable.
    mutable system::hash_list children_;
};

} // namespace blockchain
} // namespace libbitcoin

// Standard (boost) hash.
//-----------------------------------------------------------------------------

namespace boost
{

// Extend boost namespace with our header_entry hash function.
template <>
struct hash<bc::blockchain::header_entry>
{
    size_t operator()(const bc::blockchain::header_entry& entry) const
    {
        return boost::hash<bc::system::hash_digest>()(entry.hash());
    }
};

} // namespace boost

#endif
