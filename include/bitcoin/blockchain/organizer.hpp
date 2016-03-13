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
#include <cstdint>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/block_info.hpp>
#include <bitcoin/blockchain/orphan_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>

namespace libbitcoin {
namespace blockchain {

// TODO: This is not an interface, collapse with organizer_impl.
/// Structure which organises the blocks from the orphan pool to the blockchain.
class BCB_API organizer
{
public:
    typedef std::shared_ptr<organizer> ptr;
    typedef chain::block::ptr_list list;
    typedef resubscriber<const code&, uint64_t, const list&, const list&>
        reorganize_subscriber;
    typedef std::function<bool(const code&, uint64_t, const list&, const list&)>
        reorganize_handler;

    organizer(threadpool& pool, simple_chain& chain, const settings& settings);

    virtual void organize();
    virtual bool add(block_detail::ptr block);
    virtual void subscribe_reorganize(reorganize_handler handler);
    virtual void stop();

protected:
    virtual bool stopped();
    virtual code verify(uint64_t fork_index, 
        const block_detail::list& orphan_chain, uint64_t orphan_index);

private:
    typedef block_detail::list detail_list;
    static uint64_t count_inputs(const chain::block& block);

    bool strict(uint64_t fork_point);
    void process(block_detail::ptr process_block);
    void replace_chain(uint64_t fork_index, detail_list& orphan_chain);
    void clip_orphans(detail_list& orphan_chain, uint64_t orphan_index,
        const code& invalid_reason);
    void notify_reorganize(uint64_t fork_point,
        const detail_list& orphan_chain, const detail_list& replaced_chain);

    std::atomic<bool> stopped_;
    const bool use_testnet_rules_;

    // These are thread safe.
    simple_chain& chain_;
    orphan_pool orphan_pool_;
    block_detail::list process_queue_;
    config::checkpoint::list checkpoints_;
    reorganize_subscriber::ptr subscriber_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
