/*
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
#ifndef LIBBITCOIN_BLOCKCHAIN_POINTER_ARRAY_SOURCE_HPP
#define LIBBITCOIN_BLOCKCHAIN_POINTER_ARRAY_SOURCE_HPP

#include <algorithm>
#include <cstdint>
#include <boost/iostreams/categories.hpp>
#include <boost/static_assert.hpp>
#include <bitcoin/bitcoin/define.hpp>

namespace libbitcoin {
namespace blockchain {

template<typename SourceType, typename CharType>
class pointer_array_source
{
public:
    typedef CharType char_type;
    typedef boost::iostreams::source_tag category;

    pointer_array_source(const SourceType* begin, uint64_t size)
      : size_(size), position_(0), begin_(begin)
    {
        static_assert(sizeof(SourceType) == sizeof(CharType), "oops!");
    }

    std::streamsize read(char_type* s, std::streamsize n)
    {
        std::streamsize read_length = -1;

        if (position_ < size_ && n > 0)
        {
            const uint64_t positive_n = n;
            const SourceType* current_position = &begin_[position_];
            const uint64_t length = std::min(positive_n, (size_ - position_));
            std::memcpy(s, current_position, length);
            position_ += length;
            read_length = length;
        }

        return read_length;
    }

private:
    uint64_t size_;
    uint64_t position_;
    const SourceType* begin_;
};

typedef pointer_array_source<uint8_t, char> byte_pointer_array_source;

} // namespace blockchain
} // namespace libbitcoin

#endif
