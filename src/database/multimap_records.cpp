/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/blockchain/database/multimap_records.hpp>

namespace libbitcoin {
namespace chain {

multimap_records_iterator::multimap_records_iterator(
    const linked_records& linked_rows, index_type index)
  : linked_rows_(linked_rows), index_(index)
{
}

void multimap_records_iterator::operator++()
{
    index_ = linked_rows_.next(index_);
}
index_type multimap_records_iterator::operator*() const
{
    return index_;
}

multimap_iterable::multimap_iterable(
    const linked_records& linked_rows, index_type begin_index)
  : linked_rows_(linked_rows), begin_index_(begin_index)
{
}

multimap_records_iterator multimap_iterable::begin() const
{
    return multimap_records_iterator(linked_rows_, begin_index_);
}
multimap_records_iterator multimap_iterable::end() const
{
    return multimap_records_iterator(linked_rows_, linked_rows_.empty);
}

bool operator!=(
    multimap_records_iterator iter_a, multimap_records_iterator iter_b)
{
    return iter_a.index_ != iter_b.index_;
}

} // namespace chain
} // namespace libbitcoin

