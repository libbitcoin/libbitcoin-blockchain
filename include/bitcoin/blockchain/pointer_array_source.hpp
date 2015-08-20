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
//#include <iosfwd>
#include <boost/iostreams/categories.hpp>
#include <boost/static_assert.hpp>
#include <bitcoin/bitcoin/define.hpp>

namespace libbitcoin {
namespace blockchain {

template<typename SourceType, typename CharType>
class BC_API pointer_array_source
{
public:

    typedef CharType char_type;
    typedef boost::iostreams::source_tag category;

    pointer_array_source(const SourceType* begin, uint64_t size)
        : begin_(begin), pos_(0), size_(size)
    {
        BOOST_STATIC_ASSERT((sizeof(SourceType) == sizeof(CharType)));
    }

    std::streamsize read(char_type* s, std::streamsize n)
    {
        std::streamsize read_length = -1;

        if ((pos_ < size_) && (n > 0))
        {
            uint64_t positive_n = n;
            const SourceType* curr_pos = &(begin_[pos_]);
            uint64_t length = std::min(positive_n, (size_ - pos_));
            std::memcpy(s, curr_pos, length);
            pos_ += length;
            read_length = length;
        }

        return read_length;
    }

private:

    const SourceType* begin_;
    uint64_t pos_;
    uint64_t size_;
};

typedef pointer_array_source<uint8_t, char> byte_pointer_array_source;

} // namespace blockchain
} // namespace libbitcoin

#endif

