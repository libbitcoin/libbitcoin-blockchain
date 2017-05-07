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
#ifndef LIBBITCOIN_BLOCKCHAIN_HEADER_ORGANIZER_HPP
#define LIBBITCOIN_BLOCKCHAIN_HEADER_ORGANIZER_HPP

#include <atomic>
#include <future>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_header.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// Organises headers via the header pool.
class BCB_API header_organizer
{
public:
    typedef handle0 result_handler;
    typedef std::shared_ptr<header_organizer> ptr;

    /// Construct an instance.
    header_organizer(prioritized_mutex& mutex, dispatcher& dispatch,
        threadpool& thread_pool, fast_chain& chain, const settings& settings);

    bool start();
    bool stop();

    void organize(header_const_ptr header, result_handler handler);

    /// Remove all message vectors that match header hashes.
    void filter(get_data_ptr message) const;

protected:
    bool stopped() const;

private:
    // Verify sub-sequence.
    void handle_check(const code& ec, header_const_ptr header,
        result_handler handler);
    void handle_accept(const code& ec, header_const_ptr header,
        result_handler handler);

    void signal_completion(const code& ec);

    // These are thread safe.
    prioritized_mutex& mutex_;
    std::atomic<bool> stopped_;
    std::promise<code> resume_;
    ////header_pool header_pool_;
    validate_header validator_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
