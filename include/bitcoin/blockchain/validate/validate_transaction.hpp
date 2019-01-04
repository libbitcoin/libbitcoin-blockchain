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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_TRANSACTION_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_TRANSACTION_HPP

#include <atomic>
#include <cstddef>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/populate/populate_transaction.hpp>
#include <bitcoin/blockchain/settings.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API validate_transaction
{
public:
    typedef system::handle0 result_handler;

    validate_transaction(system::dispatcher& dispatch, const fast_chain& chain,
        const settings& settings);

    void start();
    void stop();

    system::code check(system::transaction_const_ptr tx,
        uint64_t max_money) const;
    void accept(system::transaction_const_ptr tx,
        result_handler handler) const;
    void connect(system::transaction_const_ptr tx,
        result_handler handler) const;

protected:
    bool stopped() const;

private:
    void handle_populated(const system::code& ec,
        system::transaction_const_ptr tx, result_handler handler) const;
    void connect_inputs(system::transaction_const_ptr tx, size_t bucket,
        size_t buckets, result_handler handler) const;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const bool retarget_;
    const bool use_libconsensus_;
    system::dispatcher& dispatch_;
    populate_transaction transaction_populator_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
