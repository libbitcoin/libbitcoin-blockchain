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
#ifndef LIBBITCOIN_BLOCKCHAIN_SLAB_ALLOCATOR_HPP
#define LIBBITCOIN_BLOCKCHAIN_SLAB_ALLOCATOR_HPP

#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

typedef uint8_t* slab_type;

class slab_allocator
{
public:
    BCB_API slab_allocator(mmfile& file, position_type sector_start);

    /**
      * Create slab.
      */
    BCB_API void initialize_new();

    /**
     * Prepare slab for usage.
     */
    BCB_API void start();

    /**
     * Allocate a slab.
     */
    BCB_API position_type allocate(size_t size);

    /**
     * Return a slab.
     */
    BCB_API slab_type get(position_type position);

    /**
     * Return a slab.
     */
    BCB_API const slab_type get(position_type position) const;

private:
    void reserve(size_t space_needed);

    mmfile& file_;
    uint8_t* data_ = nullptr;
    position_type end_ = 0;
};

    } // namespace chain
} // namespace libbitcoin

#endif

