/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_ERROR_HPP
#define LIBBITCOIN_BLOCKCHAIN_ERROR_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// Alias system code.
/// std::error_code "blockchain" category holds blockchain::error::error_t.
typedef std::error_code code;

namespace error {

enum error_t
{
    success
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

} // namespace error
} // namespace blockchain
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::blockchain::error::error)

#endif
