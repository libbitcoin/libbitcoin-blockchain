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
#ifndef LIBBITCOIN_BLOCKCHAIN_OORPHAN_POOL_MANAGER_HPP
#define LIBBITCOIN_BLOCKCHAIN_OORPHAN_POOL_MANAGER_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/pools/orphan_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
/// Organises blocks via the orphan pool to the blockchain.
class BCB_API orphan_pool_manager
{
public:
    typedef block_const_ptr_list list;

    typedef handle0 result_handler;
    typedef std::shared_ptr<orphan_pool_manager> ptr;
    typedef safe_chain::reorganize_handler reorganize_handler;
    typedef resubscriber<const code&, size_t, const list&, const list&>
        reorganize_subscriber;

    /// Construct an instance.
    orphan_pool_manager(threadpool& thread_pool, fast_chain& chain,
        orphan_pool& orphan_pool, const settings& settings);

    virtual bool start();
    virtual bool stop();

    virtual void organize(block_const_ptr block, result_handler handler);
    virtual void subscribe_reorganize(reorganize_handler handler);

protected:
    virtual bool stopped() const;

private:
    fork::ptr find_connected_fork(block_const_ptr block);

    void verify(fork::ptr fork, size_t index, result_handler handler);
    void handle_accept(const code& ec, fork::ptr fork, size_t index,
        result_handler handler);
    void handle_connect(const code& ec, fork::ptr fork, size_t index,
        result_handler handler);

    void organized(fork::ptr fork, result_handler handler);
    void notify_reorganize(size_t fork_height, const list& fork,
        const list& original);
    void complete(const code& ec, scope_lock::ptr lock,
        result_handler handler);

    // This is protected by mutex.
    fast_chain& fast_chain_;
    mutable upgrade_mutex mutex_;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const bool flush_reorganizations_;
    orphan_pool& orphan_pool_;
    threadpool thread_pool_;
    validate_block validator_;
    reorganize_subscriber::ptr subscriber_;
    mutable dispatcher dispatch_;

};

} // namespace blockchain
} // namespace libbitcoin

#endif
