/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_HEADER_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_HEADER_HPP

#include <atomic>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/populate/populate_header.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is NOT thread safe.
class BCB_API validate_header
{
public:
    typedef handle0 result_handler;

    validate_header(dispatcher& dispatch, const fast_chain& chain,
        const settings& settings);

    void start();
    void stop();

    void check(header_const_ptr header, result_handler handler) const;
    void accept(header_const_ptr header, result_handler handler) const;

protected:
    inline bool stopped() const
    {
        return stopped_;
    }

private:
    void handle_populated(const code& ec, header_const_ptr header,
        result_handler handler) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const fast_chain& fast_chain_;

    // This is thread safe becuase it is currently a stub.
    populate_header header_populator_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
