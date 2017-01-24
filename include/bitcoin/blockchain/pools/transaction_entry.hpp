/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_ENTRY_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_ENTRY_HPP

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <boost/functional/hash_fwd.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
class BCB_API transaction_entry
{
public:
    typedef std::shared_ptr<transaction_entry> ptr;
    typedef std::vector<ptr> list;

    /// Construct an entry for the pool.
    /// Never store an invalid transaction in the pool except for the cases of:
    /// double spend and input invalid due to forks change (sentinel forks).
    transaction_entry(transaction_const_ptr block);

    /// Use this construction only as a search key.
    transaction_entry(const hash_digest& hash);

    /// An anchor tx binds a subgraph to the chain and is not itself mempool.
    bool is_anchor() const;

    /// The size for the purpose of block limit computation.
    size_t size() const;

    /// The sigops for the purpose of block limit computation.
    /// This is computed based on the specified forks as pertains to BIP16.
    size_t sigops() const;

    /// The fees for the purpose of mempool reply and template optimization.
    uint64_t fees() const;

    /// The forks used for sigop count and validation of inputs.
    /// If the forks for the next block differ this must be recomputed.
    uint32_t forks() const;

    /// The hash table entry identity.
    const hash_digest& hash() const;

    /// The hash table entry's parent (prevout transaction) hashes.
    const list& parents() const;

    /// The hash table entry's child (input transaction) hashes.
    const list& children() const;

    /// Add transaction to the list of children of this transaction.
    void add_child(ptr child) const;

    /// Add transaction to the list of children of this transaction.
    void add_parent(ptr parent) const;

    /// Serializer for debugging (temporary).
    friend std::ostream& operator<<(std::ostream& out,
        const transaction_entry& of);

    /// Operators.
    bool operator==(const transaction_entry& other) const;

private:
    // These are non-const to allow for default copy construction.
    // TODO: can save 8 bytes per entry by limiting size/sigops to 32 bit.
    size_t size_;
    size_t sigops_;
    uint64_t fees_;
    uint32_t forks_;
    hash_digest hash_;

    // Used in DAG search.
    mutable bool marked_;

    // These do not affect the entry hash, so must be mutable.
    mutable list parents_;
    mutable list children_;
};

} // namespace blockchain
} // namespace libbitcoin

// Standard (boost) hash.
//-----------------------------------------------------------------------------

namespace boost
{

// Extend boost namespace with our transaction_const_ptr hash function.
template <>
struct hash<bc::blockchain::transaction_entry>
{
    size_t operator()(const bc::blockchain::transaction_entry& entry) const
    {
        return boost::hash<bc::hash_digest>()(entry.hash());
    }
};

} // namespace boost

#endif
