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
#ifndef LIBBITCOIN_BLOCKCHAIN_HSDB_SETTINGS_HPP
#define LIBBITCOIN_BLOCKCHAIN_HSDB_SETTINGS_HPP

#include <functional>
#include <bitcoin/bitcoin/types.hpp>
#include <bitcoin/bitcoin/utility/mmfile.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/types.hpp>

namespace libbitcoin {
    namespace chain {

struct BCB_API hsdb_settings
{
    size_t number_shards() const;
    size_t scan_bitsize() const;
    size_t scan_size() const;
    size_t number_buckets() const;

    size_t version = 1;
    size_t shard_max_entries = 1000000;
    size_t total_key_size = 20;
    size_t sharded_bitsize = 8;
    size_t bucket_bitsize = 8;
    size_t row_value_size = 49;
};

/**
  * Load the hsdb settings from the control file.
  */
BCB_API hsdb_settings load_hsdb_settings(const mmfile& file);

/**
  * Save the hsdb settings in the control file.
  */
BCB_API void save_hsdb_settings(
    mmfile& file, const hsdb_settings& settings);

    } // namespace chain
} // namespace libbitcoin

#endif

