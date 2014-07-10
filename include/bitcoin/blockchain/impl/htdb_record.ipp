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
#ifndef LIBBITCOIN_BLOCKCHAIN_HTDB_RECORD_IPP
#define LIBBITCOIN_BLOCKCHAIN_HTDB_RECORD_IPP

#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

template <typename HashType>
htdb_record<HashType>::htdb_record(
    htdb_record_header& header, record_allocator& allocator)
  : header_(header), allocator_(allocator)
{
}

template <typename HashType>
void htdb_record<HashType>::store(const HashType& key, write_function write)
{
}

template <typename HashType>
const record_type htdb_record<HashType>::get(const HashType& key) const
{
}

template <typename HashType>
void htdb_record<HashType>::unlink(const HashType& key)
{
}

    } // namespace chain
} // namespace libbitcoin

#endif

