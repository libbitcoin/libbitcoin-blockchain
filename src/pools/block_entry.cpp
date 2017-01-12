/**
 * Copyright (c) 2011-2016 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/pools/block_entry.hpp>

#include <algorithm>
#include <iostream>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

block_entry::block_entry(block_const_ptr block)
  : hash_(block->hash()), block_(block)
{
}

// Create a search key.
block_entry::block_entry(const hash_digest& hash)
  : hash_(hash)
{
}

block_const_ptr block_entry::block() const
{
    return block_;
}

const hash_digest& block_entry::hash() const
{
    return hash_;
}

// Not callable if the entry is a search key.
const hash_digest& block_entry::parent() const
{
    BITCOIN_ASSERT(block_);
    return block_->header().previous_block_hash();
}

// Not valid if the entry is a search key.
const hash_list& block_entry::children() const
{
    BITCOIN_ASSERT(block_);
    return children_;
}

// This is not guarded against redundant entries.
void block_entry::add_child(block_const_ptr child) const
{
    children_.push_back(child->hash());
}

////// This is not guarded against redundant entries.
////void block_entry::remove_child(block_const_ptr child) const
////{
////    auto it = std::find(children_.begin(), children_.end(), child->hash());
////
////    if (it != children_.end())
////        children_.erase(it);
////}

std::ostream& operator<<(std::ostream& out, const block_entry& of)
{
    out << encode_hash(of.hash_)
        << " " << encode_hash(of.parent())
        << " " << of.children_.size();
    return out;
}

// For the purpose of bimap identity only the tx hash matters.
bool block_entry::operator==(const block_entry& other) const
{
    return hash_ == other.hash_;
}

} // namespace blockchain
} // namespace libbitcoin
