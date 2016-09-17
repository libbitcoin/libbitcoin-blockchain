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
#ifndef LIBBITCOIN_BLOCKCHAIN_ORGANIZER_HPP
#define LIBBITCOIN_BLOCKCHAIN_ORGANIZER_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/orphan_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>
#include <bitcoin/blockchain/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is not thread safe.
/// Organises blocks via the orphan pool to the blockchain.
class BCB_API organizer
{
public:
    typedef handle0 result_handler;
    typedef std::shared_ptr<organizer> ptr;
    typedef message::block_message::ptr_list list;
    typedef resubscriber<const code&, size_t, const list&, const list&>
        reorganize_subscriber;
    typedef std::function<bool(const code&, size_t, const list&, const list&)>
        reorganize_handler;

    /// Construct an instance.
    organizer(threadpool& pool, simple_chain& chain, const settings& settings);

    /// This method is NOT thread safe.
    virtual void organize();

    virtual void start();
    virtual void stop();
    virtual bool add(block_detail::ptr block);
    virtual void subscribe_reorganize(reorganize_handler handler);
    virtual void filter_orphans(message::get_data::ptr message);

protected:
    virtual bool stopped() const;

private:
    typedef block_detail::ptr detail_ptr;
    typedef block_detail::list detail_list;

    static size_t compute_height(size_t fork_height, size_t orphan_index);

    virtual code verify(size_t fork_height, const detail_list& new_chain,
        size_t orphan_index);

    hash_number chain_work(size_t fork_height, detail_list& new_chain);
    bool strict(size_t fork_height) const;
    void process(detail_ptr block);
    void replace_chain(size_t fork_height, detail_list& new_chain);
    void remove_processed(detail_ptr block);
    void clip_orphans(detail_list& new_chain, size_t orphan_index,
        const code& reason);

    /// This method is thread safe.
    void notify_reorganize(size_t fork_height, const detail_list& new_chain,
        const detail_list& old_chain);

    std::atomic<bool> stopped_;
    const bool testnet_rules_;
    const config::checkpoint::list checkpoints_;

    // These are protected by the caller protecting organize().
    simple_chain& chain_;
    detail_list process_queue_;
    validate_block validator_;

    // These are thread safe.
    orphan_pool orphan_pool_;
    reorganize_subscriber::ptr subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
