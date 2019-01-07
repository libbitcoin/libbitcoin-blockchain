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
#include <bitcoin/blockchain/pools/block_entry.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

block_entry::block_entry(block_const_ptr block)
  : hash_(block->hash()), block_(block)
{
}

// Create a search key.
block_entry::block_entry(const hash_digest& hash)
  : hash_(hash), block_(nullptr)
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

// For the purpose of bimap identity only the block hash matters.
bool block_entry::operator==(const block_entry& other) const
{
    return hash_ == other.hash_;
}

} // namespace blockchain
} // namespace libbitcoin
