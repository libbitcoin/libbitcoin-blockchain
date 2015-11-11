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
#include <bitcoin/blockchain/checkpoint.hpp>

#include <algorithm>
#include <stdexcept>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace blockchain {

config::checkpoint::list& checkpoint::sort(config::checkpoint::list& checks)
{
    const auto comparitor = [](const config::checkpoint& left,
        const config::checkpoint& right)
    {
        return left.height() < right.height();
    };

    std::sort(checks.begin(), checks.end(), comparitor);
    return checks;
}

bool checkpoint::validate(const hash_digest& hash, const size_t height,
    const config::checkpoint::list& checks)
{
    const auto match_invalid = [&height, &hash](const config::checkpoint& item)
    {
        return height == item.height() && hash != item.hash();
    };

    const auto it = std::find_if(checks.begin(), checks.end(), match_invalid);
    return it == checks.end();
}

} // namespace chain
} // namespace libbitcoin

