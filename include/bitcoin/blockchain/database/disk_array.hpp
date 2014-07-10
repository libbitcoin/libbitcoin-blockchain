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
#ifndef LIBBITCOIN_BLOCKCHAIN_DISK_ARRAY_HPP
#define LIBBITCOIN_BLOCKCHAIN_DISK_ARRAY_HPP

#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

/**
 * Implements on disk array with a fixed size.
 *
 * File format looks like:
 *
 *  [   size:IndexType   ]
 *  [ [      ...       ] ]
 *  [ [ item:ValueType ] ]
 *  [ [      ...       ] ]
 *
 * Empty items are represented by the value array.empty
 */
template <typename IndexType, typename ValueType>
class disk_array
{
public:
    static constexpr ValueType empty = std::numeric_limits<ValueType>::max();

    /**
     * sector_start represents the offset within the file.
     */
    disk_array(mmfile& file, position_type sector_start);

    /**
     * Initialize a new array. The file must have enough space.
     * The space needed is sizeof(IndexType) + size * sizeof(ValueType)
     * Element items are initialised to disk_array::empty.
     */
    void initialize_new(IndexType size);

    /**
     * Must be called before use. Loads the size from the file.
     */
    void start();

    /**
     * Read item's value.
     */
    ValueType read(IndexType index);

    /**
     * Write value to item.
     */
    void write(IndexType index, ValueType value);

    /**
     * The array's size.
     */
    IndexType size() const;

private:
    /// Disk position of item.
    position_type item_position(IndexType index);
    /// Accessor for data.
    uint8_t* data(position_type position);

    mmfile& file_;
    position_type sector_start_;

    IndexType size_ = 0;
};

    } // namespace chain
} // namespace libbitcoin

#include <bitcoin/blockchain/impl/disk_array.ipp>

#endif

