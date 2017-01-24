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
#include <bitcoin/blockchain/pools/transaction_entry.hpp>

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

// TODO: implement size, sigops, and fees caching on chain::transaction.
// This requires the full population of transaction.validation metadata.
transaction_entry::transaction_entry(transaction_const_ptr tx)
 : size_(tx->serialized_size(message::version::level::canonical)),
   sigops_(tx->signature_operations()),
   fees_(tx->fees()),
   forks_(tx->validation.state->enabled_forks()),
   hash_(tx->hash()),
   marked_(false)
{
}

// Create a search key.
transaction_entry::transaction_entry(const hash_digest& hash)
 : size_(0),
   sigops_(0),
   fees_(0),
   forks_(0),
   hash_(hash),
   marked_(false)
{
}

// Not valid if the entry is a search key.
size_t transaction_entry::size() const
{
    return size_;
}

// Not valid if the entry is a search key.
size_t transaction_entry::sigops() const
{
    return sigops_;
}

// Not valid if the entry is a search key.
uint64_t transaction_entry::fees() const
{
    return fees_;
}

// Not valid if the entry is a search key.
uint32_t transaction_entry::forks() const
{
    return forks_;
}

// Not valid if the entry is a search key.
const hash_digest& transaction_entry::hash() const
{
    return hash_;
}

// Not valid if the entry is a search key.
const transaction_entry::list& transaction_entry::parents() const
{
    return parents_;
}

// Not valid if the entry is a search key.
const transaction_entry::list& transaction_entry::children() const
{
    return children_;
}

// This is not guarded against redundant entries.
void transaction_entry::add_child(ptr child) const
{
    children_.push_back(child);
}

// This is not guarded against redundant entries.
void transaction_entry::add_parent(ptr parent) const
{
    parents_.push_back(parent);
}

std::ostream& operator<<(std::ostream& out, const transaction_entry& of)
{
    out << encode_hash(of.hash_)
        << " " << of.parents_.size()
        << " " << of.children_.size();
    return out;
}

// For the purpose of bimap identity only the tx hash matters.
bool transaction_entry::operator==(const transaction_entry& other) const
{
    return hash_ == other.hash_;
}

} // namespace blockchain
} // namespace libbitcoin
