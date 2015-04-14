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
#ifndef LIBBITCOIN_BLOCKCHAIN_ORGANIZER_IMPL_HPP
#define LIBBITCOIN_BLOCKCHAIN_ORGANIZER_IMPL_HPP

#include <bitcoin/blockchain/checkpoint.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>

namespace libbitcoin {
namespace blockchain {

class BCB_API organizer_impl
  : public organizer
{
public:
    organizer_impl(threadpool& pool, db_interface& database,
        orphans_pool& orphans, simple_chain& chain,
        const config::checkpoint::list& checks=checkpoint::defaults);

protected:
    std::error_code verify(size_t fork_point,
        const block_detail_list& orphan_chain, size_t orphan_index);

private:
    bool strict(size_t fork_point);

    db_interface& interface_;
    config::checkpoint::list checkpoints_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
