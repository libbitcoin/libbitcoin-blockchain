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
#include <future>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/full_chain.hpp>
#include <bitcoin/blockchain/interface/simple_chain.hpp>
#include <bitcoin/blockchain/pools/orphan_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
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
    typedef full_chain::reorganize_handler reorganize_handler;
    typedef resubscriber<const code&, size_t, const list&, const list&>
        reorganize_subscriber;

    /// Construct an instance.
    orphan_pool_manager(threadpool& thread_pool, simple_chain& chain,
        orphan_pool& pool, const settings& settings);

    virtual void start();
    virtual void stop();

    virtual code organize(block_const_ptr block);
    virtual void subscribe_reorganize(reorganize_handler handler);

protected:
    virtual bool stopped() const;

private:
    static hash_number fork_difficulty(list& fork);
    static size_t to_height(size_t fork_height, size_t start_index);
    static bool validated(block_const_ptr block, size_t height);
    static void remove(list& fork, block_const_ptr block);
    static void set_result(block_const_ptr block, const code& ec);
    static void set_height(block_const_ptr block,
        size_t height=chain::block::metadata::orphan_height);
    
    code verify(block_const_ptr block, size_t height);
    code reorganize(list& fork, size_t fork_height);
    void prune(list& fork, size_t fork_height);
    void purge(list& fork, size_t start_index, const code& reason);
    void notify_reorganize(size_t fork_height, const list& fork,
        const list& original);

    void handle_connect(const code& ec, block_const_ptr block,
        size_t height, std::promise<code>& complete);

    // These are protected by the caller protecting organize().
    simple_chain& chain_;
    validate_block validator_;

    // These are thread safe.
    const bool testnet_rules_;
    const config::checkpoint::list checkpoints_;
    reorganize_subscriber::ptr subscriber_;
    std::atomic<bool> stopped_;
    orphan_pool& pool_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
