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
#include <bitcoin/blockchain/pools/utilities/transaction_pool_state.hpp>

namespace libbitcoin {
namespace blockchain {

transaction_pool_state::transaction_pool_state()
  : block_template_bytes(0), block_template_sigops(0), block_template(),
    pool(), template_byte_limit(0), template_sigop_limit(0),
    coinbase_byte_reserve(0), coinbase_sigop_reserve(0),
    cached_child_closures(), ordered_block_template()
{
}

transaction_pool_state::transaction_pool_state(const settings& )
  : transaction_pool_state()
{
}

transaction_pool_state::~transaction_pool_state()
{
    disconnect_entries();
}

void transaction_pool_state::disconnect_entries()
{
    for (auto it : pool.left)
    {
        it.first->remove_children();
        it.first->remove_parents();
    }
}

} // namespace blockchain
} // namespace libbitcoin
