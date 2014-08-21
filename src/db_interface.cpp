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
#include <bitcoin/blockchain/db_interface.hpp>

#include <boost/filesystem.hpp>
#include <bitcoin/blockchain/database/utility.hpp>

namespace libbitcoin {
    namespace chain {

const std::string path(const std::string& prefix, const std::string& filename)
{
    using boost::filesystem::path;
    path db_path = path(prefix) / filename;
    return db_path.generic_string();
}

db_paths::db_paths(const std::string& prefix)
{
    blocks_lookup = path(prefix, "blocks_lookup");
    blocks_rows = path(prefix, "blocks_rows");
    spends = path(prefix, "spends");
    transaction_map = path(prefix, "tx_map");
    transaction_index = path(prefix, "tx_index");

    history_lookup = path(prefix, "history_lookup");
    history_rows = path(prefix, "history_rows");
}

void db_paths::touch_all() const
{
    touch_file(blocks_lookup);
    touch_file(blocks_rows);
    touch_file(spends);
    touch_file(transaction_map);
    touch_file(transaction_index);
    touch_file(history_lookup);
    touch_file(history_rows);
}

db_interface::db_interface(const db_paths& paths)
  : blocks(paths.blocks_lookup, paths.blocks_rows),
    spends(paths.spends),
    transactions(paths.transaction_map, paths.transaction_index),
    history(paths.history_lookup, paths.history_rows)
{
}

void db_interface::initialize_new()
{
    blocks.initialize_new();
    spends.initialize_new();
    transactions.initialize_new();
    history.initialize_new();
}

void db_interface::start()
{
    blocks.start();
    spends.start();
    transactions.start();
    history.start();
}

    } // namespace chain
} // namespace libbitcoin

