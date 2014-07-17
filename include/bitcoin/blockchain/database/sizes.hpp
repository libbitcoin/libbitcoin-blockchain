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
#ifndef LIBBITCOIN_BLOCKCHAIN_SIZES_HPP
#define LIBBITCOIN_BLOCKCHAIN_SIZES_HPP

#include <cstdint>
#include <tuple>

namespace libbitcoin {
    namespace chain {

constexpr size_t min_slab_size = 8;
constexpr size_t min_records_size = 4;

/**
 * To calculate the record_size needed for the linked_records type, use:
 *   constexpr size_t record_size = linked_record_size + value_size;
 */
constexpr size_t linked_record_offset = 4;

constexpr size_t htdb_slab_header_size(size_t buckets)
{
    return 4 + 8 * buckets;
}

constexpr size_t htdb_record_header_size(size_t buckets)
{
    return 4 + 4 * buckets;
}

template <typename HashType>
constexpr size_t record_size_htdb(size_t value_size)
{
    return std::tuple_size<HashType>::value + 4 + value_size;
}

    } // namespace chain
} // namespace libbitcoin

#endif

