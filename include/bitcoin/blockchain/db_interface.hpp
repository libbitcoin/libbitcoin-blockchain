/**
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

#include <cstddef>
#include <boost/filesystem.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/database/spend_database.hpp>
#include <bitcoin/blockchain/database/transaction_database.hpp>
#include <bitcoin/blockchain/database/history_database.hpp>
#include <bitcoin/blockchain/database/stealth_database.hpp>

namespace libbitcoin {
namespace chain {

BC_CONSTEXPR size_t disabled_database = bc::max_size_t;

struct BCB_API db_active_heights
{
    const size_t history;
};

struct BCB_API db_paths
{
    db_paths(const boost::filesystem::path& prefix);
    bool touch_all() const;

    boost::filesystem::path blocks_lookup;
    boost::filesystem::path blocks_rows;
    boost::filesystem::path spends;
    boost::filesystem::path transactions;
    boost::filesystem::path history_lookup;
    boost::filesystem::path history_rows;
    boost::filesystem::path stealth_index;
    boost::filesystem::path stealth_rows;
};

class BCB_API db_interface
{
public:
    db_interface(const db_paths& paths,
        const db_active_heights &active_heights);

    void create();
    void start();

    void push(const block_type& block);
    block_type pop();

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
 * Create a new blockchain with a given prefix and default paths.
 */
BCB_API bool initialize_blockchain(const boost::filesystem::path& prefix);

BCB_API bool touch_file(const boost::filesystem::path& file);

} // namespace chain
} // namespace libbitcoin

#endif
