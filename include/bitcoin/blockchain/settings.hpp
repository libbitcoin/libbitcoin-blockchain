/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_BLOCKCHAIN_SETTINGS_HPP
#define LIBBITCOIN_BLOCKCHAIN_SETTINGS_HPP

#include <cstdint>
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// Common blockchain configuration settings, properties not thread safe.
class BCB_API settings
{
public:
    settings();
    settings(config::settings context);

    /// Fork flags combiner.
    uint32_t enabled_forks() const;

    /// Properties.
    uint32_t cores;
    bool priority;
    bool use_libconsensus;
    float byte_fee_satoshis;
    float sigop_fee_satoshis;
    uint64_t minimum_output_satoshis;
    uint32_t notify_limit_hours;
    uint32_t reorganization_limit;
    config::checkpoint::list checkpoints;
    bool difficult;
    bool retarget;
    bool bip16;
    bool bip30;
    bool bip34;
    bool bip66;
    bool bip65;
    bool bip90;
    bool bip68;
    bool bip112;
    bool bip113;
    bool bip141;
    bool bip143;
    bool bip147;
    bool time_warp_patch;
    bool retarget_overflow_patch;

    // Mining/Template inputs
    bc::config::script coinbase_input;
    bc::config::script coinbase_output;
    size_t block_sigop_limit;
    size_t block_bytes_limit;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
