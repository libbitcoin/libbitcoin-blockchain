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
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;

settings::settings()
  : index_payments(true),
    use_libconsensus(false),
    byte_fee_satoshis(1),
    sigop_fee_satoshis(100),
    minimum_output_satoshis(500),
    notify_limit_hours(24),
    reorganization_limit(0),
    block_buffer_limit(0)
{
}

settings::settings(chain::selection)
  : settings()
{
}

} // namespace blockchain
} // namespace libbitcoin
