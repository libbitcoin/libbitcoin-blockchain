/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_INPUT_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_INPUT_HPP

#include <cstdint>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe (static).
class BCB_API validate_input
{
public:

#ifdef WITH_CONSENSUS
    static uint32_t convert_flags(uint32_t native_forks);
    static system::code convert_result(consensus::verify_result_type result);
#endif

    static system::code verify_script(const system::chain::transaction& tx,
        uint32_t input_index, uint32_t forks, bool use_libconsensus);
};

} // namespace blockchain
} // namespace libbitcoin

#endif
