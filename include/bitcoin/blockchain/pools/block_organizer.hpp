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
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/pools/branch.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// Organises blocks via the block pool to the blockchain.
class BCB_API block_organizer
{
public:
    typedef handle0 result_handler;
    typedef std::shared_ptr<block_organizer> ptr;
    typedef safe_chain::reorganize_handler reorganize_handler;
    typedef resubscriber<code, size_t, block_const_ptr_list_const_ptr,
        block_const_ptr_list_const_ptr> reorganize_subscriber;

    /// Construct an instance.
    block_organizer(threadpool& thread_pool, fast_chain& chain,
        const settings& settings, bool relay_transactions);

    bool start();
    bool stop();
    bool close();

    void organize(block_const_ptr block, result_handler handler);
    void subscribe_reorganize(reorganize_handler&& handler);

    /// Remove all message vectors that match block hashes.
    void filter(get_data_ptr message) const;

protected:
    bool stopped() const;

private:
    // Utility.
    bool set_branch_height(branch::ptr branch);

    // Verify sub-sequence.
    void handle_accept(const code& ec, branch::ptr branch, result_handler handler);
    void handle_connect(const code& ec, branch::ptr branch, result_handler handler);
    void organized(branch::ptr branch, result_handler handler);
    void handle_reorganized(const code& ec, branch::const_ptr branch,
        block_const_ptr_list_ptr outgoing, result_handler handler);

    // Subscription.
    void notify_reorganize(size_t branch_height,
        block_const_ptr_list_const_ptr branch,
        block_const_ptr_list_const_ptr original);

    // This must be protected by the implementation.
    fast_chain& fast_chain_;

    // These are thread safe.
    std::atomic<bool> stopped_;
    block_pool block_pool_;
    threadpool priority_pool_;
    validate_block validator_;
    reorganize_subscriber::ptr subscriber_;
    mutable dispatcher priority_dispatch_;

};

} // namespace blockchain
} // namespace libbitcoin

#endif
