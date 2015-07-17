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
#ifndef LIBBITCOIN_BLOCKCHAIN_CHECKPOINTS_HPP
#define LIBBITCOIN_BLOCKCHAIN_CHECKPOINTS_HPP

#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace chain {

class checkpoints;

class BCB_API checkpoint
{
public:
    checkpoint(const std::string& hash, size_t height);
    checkpoint(size_t height, const std::string& hash);
    checkpoint(size_t height, const hash_digest& hash);
    bool invalid(size_t height, const hash_digest& hash) const;

private:
    size_t height_;
    hash_digest hash_;

    friend class checkpoints;
};

class BCB_API checkpoints
{
public:
    checkpoints(const checkpoint& top);
    bool invalid(size_t height, const hash_digest& hash) const;
    size_t last() const;

private:
    std::vector<checkpoint> checkpoints_;
};

} // namespace chain
} // namespace libbitcoin

#endif
