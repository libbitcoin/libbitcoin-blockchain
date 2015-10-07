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
#ifndef LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_TRANSACTION_POOL_HPP

#include <cstddef>
#include <functional>
#include <boost/circular_buffer.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/blockchain.hpp>

namespace libbitcoin {
namespace blockchain {

/**
 * Before bitcoin transactions make it into a block, they go into
 * a transaction memory pool. This class encapsulates that functionality
 * performing the neccessary validation of a transaction before accepting
 * it into its internal buffer.
 *
 * The interface has been deliberately kept simple to minimise overhead.
 * This class attempts no tracking of inputs or spends and only provides
 * a store/fetch paradigm. Tracking must be performed externally and make
 * use of store's handle_store and handle_confirm to manage changes in the
 * state of memory pool transactions.
 */
class BCB_API transaction_pool
{
public:
    typedef std::function<void(const code&)> exists_handler;
    typedef std::function<void(const code&)> confirm_handler;
    typedef std::function<void(const code&, const chain::index_list&)>
        validate_handler;
    typedef std::function<void(const code&, const chain::transaction&)>
        fetch_handler;

    static bool is_spent_by_tx(const chain::output_point& outpoint,
        const chain::transaction& tx);

    transaction_pool(threadpool& pool, blockchain& chain,
        size_t capacity=2000);
    ~transaction_pool();

    /// This class is not copyable.
    transaction_pool(const transaction_pool&) = delete;
    void operator=(const transaction_pool&) = delete;

    bool start();
    bool stop();

    void fetch(const hash_digest& tx_hash, fetch_handler handler);
    void validate(const chain::transaction& tx, validate_handler handler);
    void exists(const hash_digest& transaction_hash, exists_handler handler);
    void store(const chain::transaction& tx, confirm_handler confirm_handler,
        validate_handler validate_handler);

    // TODO: these should be access-limited to validate_transaction.
    bool is_in_pool(const hash_digest& tx_hash) const;
    bool is_spent_in_pool(const chain::transaction& tx) const;
    bool is_spent_in_pool(const chain::output_point& outpoint) const;
    bool find(chain::transaction& out_tx, const hash_digest& tx_hash) const;

protected:
    typedef struct entry
    {
        hash_digest hash;
        chain::transaction tx;
        confirm_handler handle_confirm;
    };
    typedef boost::circular_buffer<entry> buffer;
    typedef buffer::const_iterator iterator;
    typedef std::function<bool(const chain::input&)> input_compare;

    bool stopped();
    void do_validate(const chain::transaction& tx, validate_handler handler);
    void validation_complete(const code& ec,
        const chain::index_list& unconfirmed, const hash_digest& hash,
        validate_handler handler);
    void reorganize(const code& ec, size_t fork_point,
        const blockchain::block_list& new_blocks,
        const blockchain::block_list& replaced_blocks);
    iterator find(const hash_digest& tx_hash) const;

    void add(const chain::transaction& tx, confirm_handler handler);
    void delete_all(const code& ec);
    void delete_package(const code& ec);
    void delete_package(const hash_digest& tx_hash, const code& ec);
    void delete_package(const chain::transaction& tx, const code& ec);
    void delete_dependencies(const hash_digest& tx_hash, const code& ec);
    void delete_dependencies(const chain::output_point& point, const code& ec);
    void delete_dependencies(input_compare is_dependency, const code& ec);
    void delete_superseded(const blockchain::block_list& blocks);
    void delete_confirmed_in_blocks(const blockchain::block_list& blocks);
    void delete_spent_in_blocks(const blockchain::block_list& blocks);

    bool stopped_;
    buffer buffer_;
    dispatcher dispatch_;
    blockchain& blockchain_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
