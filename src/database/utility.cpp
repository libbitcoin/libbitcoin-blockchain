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
#include <bitcoin/blockchain/database/utility.hpp>

#include <fstream>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
    namespace chain {

void touch_file(const std::string& filename)
{
    std::ofstream outfile(filename);
    // Write byte so file is nonzero size.
    outfile.write("H", 1);
}

void reserve_space(mmfile& file, size_t required_size)
{
    if (required_size <= file.size())
        return;
    // Grow file by 1.5x
    const size_t new_size = required_size * 3 / 2;
    // Only ever grow file. Never shrink it!
    BITCOIN_ASSERT(new_size > file.size());
    bool success = file.resize(new_size);
    BITCOIN_ASSERT(success);
}

    } // namespace chain
} // namespace libbitcoin

