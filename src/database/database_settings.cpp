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
#include <bitcoin/blockchain/database/database_settings.hpp>

#include <boost/filesystem.hpp>

namespace libbitcoin {
namespace database {

using namespace boost::filesystem;

static const settings mainnet_defaults()
{
    settings value;
    value.history_start_height = 0;
    value.stealth_start_height = 350000;
    value.database_path = { "mainnet" };
    return value;
}

static const settings testnet_defaults()
{
    auto value = mainnet_defaults();
    value.stealth_start_height = 500000;
    value.database_path = { "testnet" };
    return value;
}

const settings settings::mainnet = mainnet_defaults();
const settings settings::testnet = testnet_defaults();

} // namespace database
} // namespace libbitcoin
