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
#ifndef LIBBITCOIN_BLOCKCHAIN_LINKED_RECORDS_HPP
#define LIBBITCOIN_BLOCKCHAIN_LINKED_RECORDS_HPP

#include <bitcoin/blockchain/database/record_allocator.hpp>

namespace libbitcoin {
    namespace chain {

// used by test
constexpr size_t linked_record_offset = sizeof(index_type);

/**
 * linked_records is a one-way linked list with a next value containing
 * the index of the next record.
 * Records can be dropped by forgetting an index, and updating to the next
 * value. We can think of this as a LIFO queue.
 */
class linked_records
{
public:
    // std::numeric_limits<index_type>::max()
    static constexpr index_type empty = bc::max_uint32;

    BCB_API linked_records(record_allocator& allocator);

    /**
     * Create new list with a single record.
     */
    BCB_API index_type create();

    /**
     * Insert new record before index. Returns index of new record.
     */
    BCB_API index_type insert(index_type next);

    /**
     * Read next index for record in list.
     */
    BCB_API index_type next(index_type index) const;

    /**
     * Get underlying record data.
     */
    BCB_API record_type get(index_type index) const; 

private:
    record_allocator& allocator_;
};

    } // namespace chain
} // namespace libbitcoin

#endif

