/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_TRANSACTION_ORDER_CALCULATOR_HPP
#define LIBBITCOIN_TRANSACTION_ORDER_CALCULATOR_HPP

#include <deque>
#include <bitcoin/blockchain/pools/stack_evaluator.hpp>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>

namespace libbitcoin {
namespace blockchain {

class transaction_order_calculator : public stack_evaluator
{
public:

    transaction_order_calculator();

    transaction_entry::list order_transactions();

protected:
    virtual bool visit(element_type element);

private:
    transaction_entry::list ordered_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
