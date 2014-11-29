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
#ifndef LIBBITCOIN_BLOCKCHAIN_DB_INTERFACE_HPP
#define LIBBITCOIN_BLOCKCHAIN_DB_INTERFACE_HPP

#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/database/spend_database.hpp>
#include <bitcoin/blockchain/database/transaction_database.hpp>
#include <bitcoin/blockchain/database/history_database.hpp>
#include <bitcoin/blockchain/database/stealth_database.hpp>

namespace libbitcoin {
    namespace chain {

struct db_paths
{
    BCB_API db_paths(const std::string& prefix);

    BCB_API void touch_all() const;

    std::string blocks_lookup;
    std::string blocks_rows;
    std::string spends;
    std::string transactions;

    std::string history_lookup;
    std::string history_rows;
    std::string stealth_index;
    std::string stealth_rows;
};

constexpr size_t disabled_database = std::numeric_limits<size_t>::max();

struct db_active_heights
{
    const size_t history;
};

class db_interface
{
public:
    BCB_API db_interface(const db_paths& paths,
        const db_active_heights &active_heights);

    BCB_API void initialize_new();
    BCB_API void start();

    BCB_API void push(const block_type& block);
    BCB_API block_type pop();

    block_database blocks;
    spend_database spends;
    transaction_database transactions;

    // Optional databases.
    history_database history;
    stealth_database stealth;

private:
    void push_inputs(
        const hash_digest& tx_hash, const size_t block_height,
        const transaction_input_list& inputs);
    void push_outputs(
        const hash_digest& tx_hash, const size_t block_height,
        const transaction_output_list& outputs);
    void push_stealth_outputs(
        const hash_digest& tx_hash,
        const transaction_output_list& outputs);

    void pop_inputs(
        const size_t block_height,
        const transaction_input_list& inputs);
    void pop_outputs(
        const size_t block_height,
        const transaction_output_list& outputs);

    const db_active_heights active_heights_;
};

/**
 * Convenience function to create a new blockchain with a given
 * prefix and default paths.
 */
BCB_API void initialize_blockchain(const std::string& prefix);

    } // namespace chain
} // namespace libbitcoin

#endif

