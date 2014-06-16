/*
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_FETCH_HISTORY_HPP
#define LIBBITCOIN_BLOCKCHAIN_FETCH_HISTORY_HPP

#include <bitcoin/blockchain/blockchain.hpp>

namespace libbitcoin {
    namespace chain {

template <typename DatabasePtr>
class fetch_history_t
{
public:
    fetch_history_t(DatabasePtr& db_credit, DatabasePtr& db_debit);
    blockchain::history_list operator()(
        const payment_address& address, size_t from_height);
private:
    DatabasePtr& db_credit_;
    DatabasePtr& db_debit_;
};

template <typename DatabasePtr>
fetch_history_t<DatabasePtr> fetch_history_functor(
    DatabasePtr& db_credit, DatabasePtr& db_debit);

    } // namespace chain
} // namespace libbitcoin

#include "fetch_history.ipp"

#endif

