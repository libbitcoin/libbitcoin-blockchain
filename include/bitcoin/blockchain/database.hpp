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
#ifndef LIBBITCOIN_BLOCKCHAIN_DATABASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_DATABASE_HPP

#include <cstddef>
#include <boost/filesystem.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/database/spend_database.hpp>
#include <bitcoin/blockchain/database/transaction_database.hpp>
#include <bitcoin/blockchain/database/history_database.hpp>
#include <bitcoin/blockchain/database/stealth_database.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API database
{
public:
    typedef boost::filesystem::path path;

    class store
    {
    public:

        store(const path& prefix);
        bool touch_all() const;

        path blocks_lookup;
        path blocks_rows;
        path spends;
        path transactions;
        path history_lookup;
        path history_rows;
        path stealth_index;
        path stealth_rows;
    };

    /// Create a new blockchain with a given path prefix and default paths.
    static bool initialize(const path& prefix, const chain::block& genesis);
    static bool touch_file(const path& file);

    database(const settings& settings);

    void create();
    void start();

    void push(const chain::block& block);
    chain::block pop();

    block_database blocks;
    spend_database spends;
    transaction_database transactions;
    history_database history;
    stealth_database stealth;

protected:
    database(const store& paths, size_t history_height, size_t stealth_height);
    database(const path& prefix, size_t history_height, size_t stealth_height);

private:
    typedef chain::input::list inputs;
    typedef chain::output::list outputs;

    void push_inputs(const hash_digest& tx_hash, size_t height,
        const inputs& inputs);
    void push_outputs(const hash_digest& tx_hash, size_t height,
        const outputs& outputs);
    void push_stealth(const hash_digest& tx_hash, size_t height,
        const outputs& outputs);
    void pop_inputs(const inputs& inputs, size_t height);
    void pop_outputs(const outputs& outputs, size_t height);

    const size_t history_height_;
    const size_t stealth_height_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
