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
#include <bitcoin/blockchain/organizer_impl.hpp>

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoints.hpp>
#include <bitcoin/blockchain/validate_block_impl.hpp>

namespace libbitcoin {
namespace chain {

organizer_impl::organizer_impl(db_interface& database, orphans_pool& orphans,
    simple_chain& chain, reorganize_handler handler)
  : organizer(orphans, chain), interface_(database), handler_(handler)
{
}

std::error_code organizer_impl::verify(size_t fork_index,
    const block_detail_list& orphan_chain, size_t orphan_index)
{
    BITCOIN_ASSERT(orphan_index < orphan_chain.size());
    const auto& current_block = orphan_chain[orphan_index]->actual();
    const size_t height = fork_index + orphan_index + 1;
    BITCOIN_ASSERT(height != 0);

    validate_block_impl validate(interface_, fork_index, orphan_chain,
        orphan_index, height, current_block);

    // Perform checks.
    auto code = validate.check_block();
    if (code)
        return code;

    code = validate.accept_block();
    if (code)
        return code;

    // Skip non-essential checks if before last checkpoint.
    if (fork_index < block_validation_cutoff_height)
        return std::error_code();

    // Perform strict but slow tests - connect_block()
    return validate.connect_block();
}

void organizer_impl::reorganize_occured(size_t fork_point,
    const blockchain::block_list& arrivals,
    const blockchain::block_list& replaced)
{
    handler_(std::error_code(), fork_point, arrivals, replaced);
}

} // namespace chain
} // namespace libbitcoin
