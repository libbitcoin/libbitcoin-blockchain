/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

// Sponsored in part by Digital Contract Design, LLC

#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_INITIALIZER_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_CHAIN_INITIALIZER_HPP

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <boost/filesystem.hpp>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API block_chain_initializer
  : public system::noncopyable
{
public:
    block_chain_initializer(const blockchain::settings& settings,
        const database::settings& database_settings,
        const system::settings& system_settings);

    /// Call close on destruct.
    ~block_chain_initializer();

    /// Create and open all databases.
    system::code create(const system::chain::block& genesis);

    // INITCHAIN (genesis)
    /// Push the block through candidacy and confirmation, without indexing.
    system::code push(const system::chain::block& block, size_t height=0,
        uint32_t median_time_past=0);

    database::data_base& database();
    const blockchain::settings& chain_settings();
    const database::settings& database_settings();
    const system::settings& system_settings();

protected:
    system::code populate_neutrino_filter_metadata(
        const system::chain::block& block, size_t height);

private:
    database::data_base database_;
    const blockchain::settings& settings_chain_;
    const database::settings& settings_database_;
    const system::settings& settings_system_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
