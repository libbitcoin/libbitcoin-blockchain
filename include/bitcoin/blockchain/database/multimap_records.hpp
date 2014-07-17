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

class multimap_records_iterator
{
public:
    BCB_API multimap_records_iterator(
        linked_records& linked_rows, index_type index);

    BCB_API void operator++();
    BCB_API index_type operator*() const;

private:
    friend bool operator!=(
        multimap_records_iterator iter_a, multimap_records_iterator iter_b);

    linked_records& linked_rows_;
    index_type index_;
};

BCB_API bool operator!=(
    multimap_records_iterator iter_a, multimap_records_iterator iter_b);

class multimap_lookup_result
{
public:
    BCB_API multimap_lookup_result(
        linked_records& linked_rows, index_type begin_index);

    BCB_API multimap_records_iterator begin() const;
    BCB_API multimap_records_iterator end() const;

private:
    linked_records& linked_rows_;
    index_type begin_index_;
};

template <typename HashType>
class multimap_records
{
public:
    typedef htdb_record<HashType> htdb_type;
    typedef std::function<void (uint8_t*)> write_function;

    multimap_records(htdb_type& map, linked_records& linked_rows);

    multimap_lookup_result lookup(const HashType& key) const;
    void add_row(const HashType& key, write_function write);
    void delete_last_row(const HashType& key);

private:
    void add_to_list(record_type start_info,
        const HashType& key, write_function write);
    void create_new(const HashType& key, write_function write);

    htdb_type& map_;
    linked_records& linked_rows_;
};

    } // namespace chain
} // namespace libbitcoin

#include <bitcoin/blockchain/impl/multimap_records.ipp>

#endif

