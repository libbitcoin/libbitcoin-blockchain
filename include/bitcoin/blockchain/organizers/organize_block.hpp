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
#ifndef LIBBITCOIN_BLOCKCHAIN_ORGANIZE_BLOCK_HPP
#define LIBBITCOIN_BLOCKCHAIN_ORGANIZE_BLOCK_HPP

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// Organises blocks to the store.
class BCB_API organize_block
{
public:
    typedef system::handle0 result_handler;
    typedef std::shared_ptr<organize_block> ptr;
    typedef std::function<bool(system::code, system::block_const_ptr, size_t)>
        download_handler;
    typedef system::resubscriber<system::code, system::hash_digest, size_t>
        download_subscriber;

    /// Construct an instance.
    organize_block(system::prioritized_mutex& mutex,
        system::dispatcher& priority_dispatch, system::threadpool& threads,
        fast_chain& chain, block_pool& pool, const settings& settings,
        const system::settings& bitcoin_settings);

    // Start/stop the organizer.
    bool start();
    bool stop();

    /// validate and organize a block into the store.
    system::code organize(system::block_const_ptr block, size_t height);

    /// Push a validatable block identifier onto the download subscriber.
    void prime_validation(const system::hash_digest& hash,
        size_t height) const;

protected:
    bool stopped() const;

private:
    // Verify sub-sequence.
    system::code validate(system::block_const_ptr block);
    bool handle_check(const system::code& ec, const system::hash_digest& hash,
        size_t height);
    void handle_accept(const system::code& ec, system::block_const_ptr block,
        result_handler handler);
    void handle_connect(const system::code& ec, system::block_const_ptr block,
        result_handler handler);
    void signal_completion(const system::code& ec);

    // These are thread safe.
    fast_chain& fast_chain_;
    system::prioritized_mutex& mutex_;
    std::atomic<bool> stopped_;
    std::promise<system::code> resume_;
    block_pool& pool_;
    validate_block validator_;
    download_subscriber::ptr downloader_subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
