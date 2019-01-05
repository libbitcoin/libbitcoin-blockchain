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
#include <bitcoin/blockchain/pools/header_entry.hpp>

#include <cstddef>
////#include <iostream>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

header_entry::header_entry(header_const_ptr header, size_t height)
  : height_(height), hash_(header->hash()), header_(header)
{
}

// Create a search key.
header_entry::header_entry(const hash_digest& hash)
  : height_(0), hash_(hash)
{
}

header_const_ptr header_entry::header() const
{
    return header_;
}

size_t header_entry::height() const
{
    return height_;
}

const hash_digest& header_entry::hash() const
{
    return hash_;
}

// Not callable if the entry is a search key.
const hash_digest& header_entry::parent() const
{
    BITCOIN_ASSERT(header_);
    return header_->previous_block_hash();
}

// Not valid if the entry is a search key.
const hash_list& header_entry::children() const
{
    ////BITCOIN_ASSERT(header_);
    return children_;
}

// This is not guarded against redundant entries.
void header_entry::add_child(header_const_ptr child) const
{
    children_.push_back(child->hash());
}

// For the purpose of bimap identity only the header hash matters.
bool header_entry::operator==(const header_entry& other) const
{
    return hash_ == other.hash_;
}

////std::ostream& operator<<(std::ostream& out, const header_entry& of)
////{
////    out << encode_hash(of.hash_)
////        << " " << encode_hash(of.parent())
////        << " " << of.children_.size();
////    return out;
////}

} // namespace blockchain
} // namespace libbitcoin
