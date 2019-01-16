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
    typedef std::function<void(system::block_const_ptr)> block_result_handler;
    typedef system::resubscriber<system::code, size_t> download_subscriber;

    /// Construct an instance.
    organize_block(system::prioritized_mutex& mutex,
        system::dispatcher& priority_dispatch, system::threadpool& threads,
        fast_chain& chain, block_pool& pool, const settings& settings,
        const system::settings& bitcoin_settings);

    /// Start/stop the organizer.
    bool start();
    bool stop();

    /// validate and organize a block into the store.
    system::code organize(system::block_const_ptr block, size_t height);

    /// Push a validatable block height onto the download subscriber.
    void prime_validation(size_t height) const;

protected:
    bool stopped() const;

private:
    // TODO: create common definitions.
    typedef system::handle1<system::block_const_ptr> read_handler;
    typedef std::shared_ptr<system::block_const_ptr_list>
        block_const_ptr_list_ptr;

    // Validate sequence.
    void handle_complete(const system::code& ec);
    void block_fetcher(size_t height, const system::hash_digest& parent_hash,
        block_const_ptr_list_ptr sub_branch, result_handler complete);
    void handle_fetch(const system::code& ec, system::block_const_ptr block,
        size_t height, const system::hash_digest& parent_hash,
        block_const_ptr_list_ptr sub_branch, result_handler complete);

    // Validate sub-sequence.
    system::code validate(system::block_const_ptr block);
    bool handle_check(const system::code& ec, size_t height);
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
    std::promise<system::block_const_ptr> resume_block_;
    block_pool& pool_;
    system::dispatcher& dispatch_;
    validate_block validator_;
    download_subscriber::ptr downloader_subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
