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
#ifndef LIBBITCOIN_PARENT_CLOSURE_CALCULATOR_HPP
#define LIBBITCOIN_PARENT_CLOSURE_CALCULATOR_HPP

#include <deque>
#include <bitcoin/blockchain/pools/stack_evaluator.hpp>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>
#include <bitcoin/blockchain/pools/transaction_pool_state.hpp>

namespace libbitcoin {
namespace blockchain {

class parent_closure_calculator : public stack_evaluator
{
public:

    parent_closure_calculator(transaction_pool_state& state);

    transaction_entry::list get_closure(transaction_entry::ptr tx);

protected:
    virtual bool visit(transaction_entry::ptr element);
};

} // namespace blockchain
} // namespace libbitcoin

#endif
