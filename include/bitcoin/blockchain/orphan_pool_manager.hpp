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
#include <bitcoin/blockchain/full_chain.hpp>
#include <bitcoin/blockchain/orphan_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>
#include <bitcoin/blockchain/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
/// Organises blocks via the orphan pool to the blockchain.
class BCB_API orphan_pool_manager
{
public:
    typedef handle0 result_handler;
    typedef std::shared_ptr<orphan_pool_manager> ptr;
    typedef full_chain::reorganize_handler reorganize_handler;

    typedef resubscriber<const code&, size_t, const block_const_ptr_list&,
        const block_const_ptr_list&> reorganize_subscriber;

    /// Construct an instance.
    orphan_pool_manager(threadpool& pool, simple_chain& chain,
        const settings& settings);

    virtual void start();
    virtual void stop();

    virtual code reorganize(block_const_ptr block);
    virtual void filter_orphans(get_data_ptr message) const;
    virtual void subscribe_reorganize(reorganize_handler handler);

protected:
    virtual bool stopped() const;

private:
    static size_t compute_height(size_t fork_height, size_t orphan_index);

    // const
    code verify(size_t fork_height, const block_const_ptr_list& new_chain,
        size_t orphan_index) const;
    bool strict(size_t fork_height) const;

    // non-const
    void process(block_const_ptr block);
    void chain_work(hash_number& work, block_const_ptr_list& new_chain,
        size_t fork_height);
    void replace_chain(block_const_ptr_list& new_chain, size_t fork_height);
    void clip_orphans(block_const_ptr_list& new_chain, size_t orphan_index,
        const code& reason);
    void remove_processed(block_const_ptr block);

    /// This method is thread safe.
    void notify_reorganize(size_t fork_height,
        const block_const_ptr_list& new_chain,
        const block_const_ptr_list& old_chain);


    // These are protected by the caller protecting organize().
    simple_chain& chain_;
    block_const_ptr_list process_queue_;
    validate_block validator_;

    // These are thread safe.
    const bool testnet_rules_;
    const config::checkpoint::list checkpoints_;
    std::atomic<bool> stopped_;
    orphan_pool orphan_pool_;
    reorganize_subscriber::ptr subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
