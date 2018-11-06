/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_STACK_EVALUATOR_HPP
#define LIBBITCOIN_BLOCKCHAIN_STACK_EVALUATOR_HPP

#include <deque>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>

namespace libbitcoin {
namespace blockchain {

class stack_evaluator
{
public:
    typedef transaction_entry::ptr element_type;

    typedef std::map<hash_digest, element_type> element_type_natural_key_map;

    void enqueue(element_type element);

protected:
    virtual bool visit(element_type element) = 0;

    void evaluate();

    bool has_encountered(element_type element) const;

    void mark_encountered(element_type element);

    element_type_natural_key_map::const_iterator begin_encountered() const;

    element_type_natural_key_map::const_iterator end_encountered() const;

private:
    element_type_natural_key_map encountered_;
    std::deque<element_type> stack_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
