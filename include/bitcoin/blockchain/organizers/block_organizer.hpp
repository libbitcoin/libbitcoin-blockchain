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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_ORGANIZER_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_ORGANIZER_HPP

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// Organises blocks to the store.
class BCB_API block_organizer
{
public:
    typedef handle0 result_handler;
    typedef std::shared_ptr<block_organizer> ptr;
    typedef std::function<bool(code, block_const_ptr, size_t)> download_handler;
    typedef resubscriber<code, block_const_ptr, size_t> download_subscriber;

    /// Construct an instance.
    block_organizer(prioritized_mutex& mutex, dispatcher& dispatch,
        threadpool& thread_pool, fast_chain& chain, const settings& settings);

    // Start/stop the organizer.
    bool start();
    bool stop();

    /// validate and organize a block into the store.
    code organize(block_const_ptr block, size_t height);

protected:
    bool stopped() const;

private:
    // Verify sub-sequence.
    bool handle_check(const code& ec, block_const_ptr block, size_t height);
    void handle_accept(const code& ec, block_const_ptr block, result_handler handler);
    void handle_connect(const code& ec, block_const_ptr block, result_handler handler);
    void signal_completion(const code& ec);

    // These are thread safe.
    fast_chain& fast_chain_;
    prioritized_mutex& mutex_;
    std::atomic<bool> stopped_;
    std::promise<code> resume_;
    validate_block validator_;
    download_subscriber::ptr downloader_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
