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
#ifndef LIBBITCOIN_PRIORITY_CALCULATOR_HPP
#define LIBBITCOIN_PRIORITY_CALCULATOR_HPP

#include <deque>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>
#include <bitcoin/blockchain/pools/stack_evaluator.hpp>

namespace libbitcoin {
namespace blockchain {

class priority_calculator : public stack_evaluator
{
public:

    priority_calculator();

    // Returns a pair containing the cumulative fees and cumulative size.
    std::pair<uint64_t, size_t> prioritize();

    uint64_t get_cumulative_fees() const;

    size_t get_cumulative_size() const;

protected:
    virtual bool visit(transaction_entry::ptr element);

private:
    uint64_t cumulative_fees_;
    size_t cumulative_size_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
