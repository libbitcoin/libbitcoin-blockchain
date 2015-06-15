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
#ifndef LIBBITCOIN_BLOCKCHAIN_CHECKPOINTS_HPP
#define LIBBITCOIN_BLOCKCHAIN_CHECKPOINTS_HPP

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace blockchain {

/**
 * Blocks before this height are not fully validated using slower
 * checks, speeding up blockchain sync speed.
 */
BC_CONSTEXPR size_t block_validation_cutoff_height = 360500;

bool passes_checkpoints(size_t height, const hash_digest& block_hash);

} // namespace blockchain
} // namespace libbitcoin

#endif
