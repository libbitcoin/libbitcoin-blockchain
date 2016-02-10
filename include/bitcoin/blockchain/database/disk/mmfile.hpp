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
#ifndef LIBBITCOIN_DATABASE_MMFILE_HPP
#define LIBBITCOIN_DATABASE_MMFILE_HPP

#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <cstddef>
#include <cstdint>
#include <string>
#include <boost/filesystem.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace database {

class BCD_API mmfile
{
public:
    mmfile(const boost::filesystem::path& filename);
    mmfile(mmfile&& file);
    ~mmfile();

    mmfile(const mmfile&) = delete;
    void operator=(const mmfile&) = delete;

    uint8_t* data();
    const uint8_t* data() const;
    size_t size() const;
    bool reserve(size_t size);
    bool resize(size_t new_size);

private:
    size_t file_size(int file_handle);
    bool map(size_t size);
    int open_file(const boost::filesystem::path& filename);
#ifdef MREMAP_MAYMOVE
    bool remap(size_t new_size);
#endif
    bool unmap();
    bool validate(size_t size);

    int file_handle_ = 0;
    uint8_t* data_ = nullptr;
    size_t size_ = 0;

    // for logging
    boost::filesystem::path filename_;
};

} // namespace database
} // namespace libbitcoin

#endif
