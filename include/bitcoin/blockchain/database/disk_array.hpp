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

template <typename IndexType, typename ValueType>
class disk_array
{
public:
    static constexpr ValueType empty = std::numeric_limits<ValueType>::max();

    disk_array(mmfile& file, position_type sector_start);

    void initialize_new(IndexType size);

    void start();

    ValueType read(IndexType index);

    void write(IndexType index, ValueType value);

private:
    mmfile& file_;
    uint8_t* data_ = nullptr;
    IndexType size_ = 0;
};

    } // namespace chain
} // namespace libbitcoin

#include <bitcoin/blockchain/impl/disk_array.ipp>

#endif

