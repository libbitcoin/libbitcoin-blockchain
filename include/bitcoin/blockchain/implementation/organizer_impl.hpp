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
#ifndef LIBBITCOIN_BLOCKCHAIN_ORGANIZER_IMPL_HPP
#define LIBBITCOIN_BLOCKCHAIN_ORGANIZER_IMPL_HPP

#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>

namespace libbitcoin {
namespace chain {

class BCB_API organizer_impl
  : public organizer
{
public:
    typedef blockchain::reorganize_handler reorganize_handler;

    organizer_impl(db_interface& database, orphans_pool& orphans,
        simple_chain& chain, reorganize_handler handler);

protected:
    std::error_code verify(size_t fork_index,
        const block_detail_list& orphan_chain, size_t orphan_index);

    void reorganize_occured(size_t fork_point,
        const blockchain::block_list& arrivals,
        const blockchain::block_list& replaced);

private:
    db_interface& interface_;
    reorganize_handler handler_;
};

} // namespace chain
} // namespace libbitcoin

#endif
