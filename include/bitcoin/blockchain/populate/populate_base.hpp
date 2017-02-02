/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_POPULATE_BASE_HPP
#define LIBBITCOIN_BLOCKCHAIN_POPULATE_BASE_HPP

#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is NOT thread safe.
class BCB_API populate_base
{
protected:
    typedef handle0 result_handler;

    populate_base(threadpool& pool, const fast_chain& chain);

    void populate_duplicate(size_t maximum_height,
        const chain::transaction& tx, bool require_confirmed) const;

    void populate_prevout(size_t maximum_height,
        const chain::output_point& outpoint, bool require_confirmed) const;

    // This is thread safe.
    mutable dispatcher dispatch_;

    // The store is protected by caller not invoking populate concurrently.
    const fast_chain& fast_chain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
