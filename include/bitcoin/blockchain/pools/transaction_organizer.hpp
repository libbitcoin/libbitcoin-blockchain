/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_ORGANIZER_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_ORGANIZER_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
/// Organises transactions via the transaction pool to the blockchain.
class BCB_API transaction_organizer
{
public:
    typedef handle0 result_handler;
    typedef std::shared_ptr<transaction_organizer> ptr;
    typedef safe_chain::transaction_handler transaction_handler;
    typedef resubscriber<code, transaction_const_ptr> transaction_subscriber;

    /// Construct an instance.
    transaction_organizer(threadpool& thread_pool, fast_chain& chain,
        const settings& settings);

    virtual bool start();
    virtual bool stop();

    virtual void organize(transaction_const_ptr tx, result_handler handler);
    virtual void subscribe_transaction(transaction_handler&& handler);

    virtual void fetch_inventory(size_t size,
        safe_chain::inventory_fetch_handler handler) const;

protected:
    virtual bool stopped() const;

private:
    fast_chain& fast_chain_;

    // These are thread safe.
    std::atomic<bool> stopped_;
    const bool flush_writes_;
    transaction_pool transaction_pool_;
    validate_transaction validator_;
    transaction_subscriber::ptr subscriber_;
    mutable dispatcher dispatch_;

};

} // namespace blockchain
} // namespace libbitcoin

#endif
