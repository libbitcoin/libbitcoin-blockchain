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
#ifndef LIBBITCOIN_BLOCKCHAIN_MULTIMAP_RECORDS_HPP
#define LIBBITCOIN_BLOCKCHAIN_MULTIMAP_RECORDS_HPP

#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>
#include <bitcoin/blockchain/database/linked_records.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>

namespace libbitcoin {
    namespace chain {

/**
 * Forward iterator for multimap record values.
 * After performing a lookup of a key, we can iterate the
 * multiple values in a for loop.
 *
 * @code
 *  for (const index_type idx: multimap.lookup(key))
 *      const record_type rec = linked_recs.get(idx);
 * @endcode
 */
class multimap_records_iterator
{
public:
    BCB_API multimap_records_iterator(
        const linked_records& linked_rows, index_type index);

    /**
     * Next value in the chain.
     */
    BCB_API void operator++();

    /**
     * Dereference the record index.
     */
    BCB_API index_type operator*() const;

private:
    friend bool operator!=(
        multimap_records_iterator iter_a, multimap_records_iterator iter_b);

    const linked_records& linked_rows_;
    index_type index_;
};

/**
 * Compare too multimap value iterators for (lack of) equivalency.
 */
BCB_API bool operator!=(
    multimap_records_iterator iter_a, multimap_records_iterator iter_b);

/**
 * Result of a multimap database query. This is a container wrapper
 * allowing the values to be iteratable.
 */
class multimap_iterable
{
public:
    BCB_API multimap_iterable(
        const linked_records& linked_rows, index_type begin_index);

    BCB_API multimap_records_iterator begin() const;
    BCB_API multimap_records_iterator end() const;

private:
    const linked_records& linked_rows_;
    index_type begin_index_;
};

/**
 * A multimap hashtable where each key maps to a set of fixed size
 * values.
 *
 * The database is abstracted on top of a record map, and linked records.
 * The map links keys to start indexes in the linked records.
 * The linked records are chains of records that can be iterated through
 * given a start index.
 */
template <typename HashType>
class multimap_records
{
public:
    typedef htdb_record<HashType> htdb_type;
    typedef std::function<void (uint8_t*)> write_function;

    multimap_records(htdb_type& map, linked_records& linked_rows);

    /**
     * Lookup a key, returning an iterable result with multiple values.
     */
    index_type lookup(const HashType& key) const;

    /**
     * Add a new row for a key. If the key doesn't exist, it will be
     * created. If it does exist, the value will be added at the start
     * of the chain.
     */
    void add_row(const HashType& key, write_function write);

    /**
     * Delete the last row entry that was added. This means when deleting
     * blocks we must walk backwards and delete in reverse order.
     */
    void delete_last_row(const HashType& key);

private:
    /// Add new value to existing key.
    void add_to_list(record_type start_info, write_function write);
    /// Create new key with a single value.
    void create_new(const HashType& key, write_function write);

    htdb_type& map_;
    linked_records& linked_rows_;
};

    } // namespace chain
} // namespace libbitcoin

#include <bitcoin/blockchain/impl/multimap_records.ipp>

#endif

