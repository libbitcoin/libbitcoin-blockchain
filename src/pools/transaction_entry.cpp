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
#include <bitcoin/blockchain/pools/transaction_entry.hpp>

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

// Space optimization since valid sigops and size are never close to 32 bits.
inline uint32_t cap(size_t value)
{
    return domain_constrain<uint32_t>(value);
}

// TODO: incorporate tx weight.
// TODO: implement size, sigops, and fees caching on chain::transaction.
// This requires the full population of transaction.metadata metadata.
transaction_entry::transaction_entry(transaction_const_ptr tx)
 : size_(cap(tx->serialized_size(message::version::level::canonical))),
   sigops_(cap(tx->signature_operations())),
   fees_(tx->fees()),
   forks_(tx->metadata.state->enabled_forks()),
   hash_(tx->hash()),
   parents_(),
   children_()
{
}

// Create a search key.
transaction_entry::transaction_entry(const hash_digest& hash)
 : size_(0),
   sigops_(0),
   fees_(0),
   forks_(0),
   hash_(hash),
   parents_(),
   children_()
{
}

transaction_entry::~transaction_entry()
{
//    remove_children();
//    remove_parents();
}

bool transaction_entry::is_anchor() const
{
    return parents_.empty();
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
size_t transaction_entry::sigops() const
{
    return sigops_;
}

// Not valid if the entry is a search key.
size_t transaction_entry::size() const
{
    return size_;
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
const transaction_entry::indexed_list& transaction_entry::children() const
{
    return children_;
}

// This is not guarded against redundant entries.
void transaction_entry::add_parent(ptr parent)
{
    parents_.push_back(parent);
}

void transaction_entry::remove_parent(const hash_digest& parent,
    bool all_instances)
{
    for (auto it = parents_.begin(); it != parents_.end();)
    {
        if ((*it)->hash() == parent)
        {
            it = parents_.erase(it);
            if (!all_instances)
                break;
        }
        else
            ++it;
    }
}

void transaction_entry::remove_parents()
{
    auto self = std::make_shared<transaction_entry>(*this);
    auto parents = parents_;

    for (auto parent: parents)
        parent->remove_child(self);

    parents_.clear();
}

void transaction_entry::add_child(uint32_t index, ptr child)
{
    children_.insert({ index, child });
}

// This is guarded against missing entries.
void transaction_entry::remove_child(uint32_t index)
{
    const auto it = children_.left.find(index);

    if (it != children_.left.end())
    {
        auto& self = hash();
        it->second->remove_parent(self, false);
        children_.erase(indexed_list::value_type(it->first, it->second));
    }
}

// This is guarded against missing entries.
void transaction_entry::remove_child(ptr child)
{
    const auto it = children_.right.find(child);

    if (it != children_.right.end())
    {
        auto& self = hash();
        it->first->remove_parent(self, true);
        children_.erase(indexed_list::value_type(it->second, it->first));
    }
}

void transaction_entry::remove_children()
{
    auto& self = hash();
    for (auto it = children_.left.begin(); it != children_.left.end(); ++it)
        it->second->remove_parent(self, true);

    children_.clear();
}

std::size_t hash_value(const transaction_entry::ptr& instance)
{
    if (!instance)
        return 0;

    boost::hash<hash_digest> hasher;
    return hasher(instance->hash());
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

bool transaction_entry::ptr_less::operator()(
    const transaction_entry::ptr& left,
    const transaction_entry::ptr& right) const
{
    if (left && right)
        return left->hash() < right->hash();
    else if (!left && right)
        return true;
    else if (left && !right)
        return false;
    else
        return false;
}

bool transaction_entry::ptr_equal::operator()(
    const transaction_entry::ptr& left,
    const transaction_entry::ptr& right) const
{
    if (left && right)
        return (*left) == (*right);
    else if (!left && right)
        return false;
    else if (left && !right)
        return false;
    else
        return true;
}

} // namespace blockchain
} // namespace libbitcoin
