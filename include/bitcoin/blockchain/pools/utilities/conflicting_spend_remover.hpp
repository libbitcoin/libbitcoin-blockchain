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
#ifndef LIBBITCOIN_BLOCKCHAIN_CONFLICTING_SPEND_REMOVER_HPP
#define LIBBITCOIN_BLOCKCHAIN_CONFLICTING_SPEND_REMOVER_HPP

#include <deque>
#include <bitcoin/blockchain/pools/utilities/stack_evaluator.hpp>
#include <bitcoin/blockchain/pools/utilities/transaction_pool_state.hpp>

namespace libbitcoin {
namespace blockchain {

class conflicting_spend_remover : public stack_evaluator
{
public:
    typedef double priority;

    conflicting_spend_remover(transaction_pool_state& state);

    priority deconflict();

protected:
    virtual bool visit(element_type element);

private:
    priority max_removed_;
    transaction_pool_state& state_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
