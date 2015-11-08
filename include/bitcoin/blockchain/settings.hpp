/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-blockchain.
 *
 * libbitcoin-blockchain is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_BLOCKCHAIN_SETTINGS_HPP
#define LIBBITCOIN_BLOCKCHAIN_SETTINGS_HPP

#include <cstdint>
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/checkpoint.hpp>

namespace libbitcoin {
namespace blockchain {

/// default settings
#define BLOCKCHAIN_THREADS                  6
#define BLOCKCHAIN_BLOCK_POOL_CAPACITY      50
#define BLOCKCHAIN_HISTORY_START_HEIGHT     0
#define BLOCKCHAIN_DATABASE_PATH            boost::filesystem::path("blockchain")

/// mainnet settings
#define BLOCKCHAIN_TESTNET_RULES_MAINNET    false
#define BLOCKCHAIN_CHECKPOINTS_MAINNET      bc::blockchain::checkpoint::mainnet

/// testnet settings
#define BLOCKCHAIN_TESTNET_RULES_TESTNET    true
#define BLOCKCHAIN_CHECKPOINTS_TESTNET      bc::blockchain::checkpoint::testnet

struct BCB_API settings
{
    uint32_t threads;
    uint32_t block_pool_capacity;
    uint32_t history_start_height;
    bool use_testnet_rules;
    boost::filesystem::path database_path;
    config::checkpoint::list checkpoints;
};

} // namespace chain
} // namespace libbitcoin

#endif
