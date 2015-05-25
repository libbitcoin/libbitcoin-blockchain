/**
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCKCHAIN_INTERFACE_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCKCHAIN_INTERFACE_HPP

/**
 * Represents an interface to a blockchain backend.
 */

#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace chain {

enum class point_ident
{
    output,
    spend
};

struct BCB_API history_row
{
    /// Is this an output or spend
    point_ident id;
    /// input/output point
    point_type point;
    /// Block height of transaction
    uint64_t height;
    union
    {
        /// If output, then satoshis value of output.
        uint64_t value;
        /**
         * If spend, then checksum hash of previous output point
         * To match up this row with the output, recompute the
         * checksum from the output row with:
         * @code
         *  spend_checksum(row.point)
         * @endcode
         */
        uint64_t previous_checksum;
    };
};

typedef std::vector<history_row> history_list;

/**
 * Create checksum so spend can be matched with corresponding
 * output point without needing the whole previous outpoint.
 */
BCB_API uint64_t spend_checksum(output_point outpoint);

struct BCB_API stealth_row
{
    // No sign byte in public key.
    hash_digest ephemkey;
    // No version byte in address.
    short_hash address;
    hash_digest transaction_hash;
};

typedef std::vector<stealth_row> stealth_list;

class blockchain
{
public:
    typedef std::function<void (const std::error_code&, block_info)>
        store_block_handler;

    typedef std::function<void (const std::error_code&)> import_block_handler;

    template <typename Message>
    using fetch_handler = std::function<
        void (const std::error_code&, const Message&)>;

    typedef fetch_handler<block_header_type> fetch_handler_block_header;

    typedef fetch_handler<hash_list> fetch_handler_block_transaction_hashes;

    typedef fetch_handler<uint64_t> fetch_handler_block_height;

    typedef fetch_handler<uint64_t> fetch_handler_last_height;

    typedef fetch_handler<block_locator_type> fetch_handler_block_locator;

    typedef fetch_handler<transaction_type> fetch_handler_transaction;

    typedef std::function<void (const std::error_code&, uint64_t, uint64_t)>
        fetch_handler_transaction_index;

    typedef fetch_handler<input_point> fetch_handler_spend;

    typedef std::function<void (const std::error_code&, const history_list&)>
        fetch_handler_history;

    typedef std::function<void (const std::error_code&, const stealth_list&)>
        fetch_handler_stealth;

    typedef std::vector<std::shared_ptr<block_type>> block_list;

    typedef std::function<void (const std::error_code&, uint64_t,
        const block_list&, const block_list&)> reorganize_handler;

    BCB_API virtual ~blockchain() {};

    /**
     * Store a new block.
     *
     * Subscriber is notified exactly once of changes to the blockchain
     * and needs to re-subscribe to continue being notified.
     *
     * @param[in]   block           Block to store
     * @param[in]   handle_store    Completion handler for store operation.
     * @code
     *  void handle_store(
     *      const std::error_code& ec,   // Status of operation
     *      block_info info              // Status and height of block
     *  );
     * @endcode
     */
    BCB_API virtual void store(const block_type& block,
        store_block_handler handle_store) = 0;

    /**
     * Store a new block directly without validating it.
     * No checks are done. Importing an already stored block
     * is undefined.
     *
     * @param[in]   import_block    Block to store
     * @param[in]   handle_import   Completion handler for import operation.
     * @code
     *  void handle_import(
     *      const std::error_code& ec   // Status of operation
     *  );
     * @encode
     */
    BCB_API virtual void import(const block_type& import_block,
        import_block_handler handle_import) = 0;

    /**
     * Fetches the block header by height.
     *
     * @param[in]   height          Height of block to fetch
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,      // Status of operation
     *      const block_header_type& blk    // Block header
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_block_header(uint64_t height,
        fetch_handler_block_header handle_fetch) = 0;

    /**
     * Fetches the block header by hash.
     *
     * @param[in]   hash            Block hash
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,      // Status of operation
     *      const block_header_type& blk    // Block header
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_block_header(const hash_digest& hash,
        fetch_handler_block_header handle_fetch) = 0;

    /**
     * Fetches list of transaction hashes in a block by block hash.
     *
     * @param[in]   hash            Block hash
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,      // Status of operation
     *      const hash_list& hashes  // List of hashes
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_block_transaction_hashes(
        const hash_digest& hash,
        fetch_handler_block_transaction_hashes handle_fetch) = 0;

    /**
     * Fetches the height of a block given its hash.
     *
     * @param[in]   hash            Block hash
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec, // Status of operation
     *      uint64_t block_height      // Height of block
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_block_height(const hash_digest& hash,
        fetch_handler_block_height handle_fetch) = 0;

    /**
     * Fetches the height of the last block in our blockchain.
     *
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec, // Status of operation
     *      uint64_t block_height      // Height of last block
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_last_height(
        fetch_handler_last_height handle_fetch) = 0;

    /**
     * Fetches a transaction by hash.
     *
     * @param[in]   hash            Transaction hash
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,  // Status of operation
     *      const transaction_type& tx  // Transaction
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_transaction(const hash_digest& hash,
        fetch_handler_transaction handle_fetch) = 0;

    /**
     * Fetch the block height that contains a transaction and its index
     * within a block.
     *
     * @param[in]   hash            Transaction hash
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec, // Status of operation
     *      uint64_t block_height,     // Height of block containing
     *                                 // the transaction.
     *      uint64_t index             // Index of transaction within
     *                                 // the block.
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_transaction_index(
        const hash_digest& hash,
        fetch_handler_transaction_index handle_fetch) = 0;

    // fetch of inputs and outputs is a future possibility
    // for now use fetch_transaction and lookup input/output

    /**
     * Fetches a corresponding spend of an output.
     *
     * @param[in]   outpoint        Representation of an output.
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,      // Status of operation
     *      const input_point& inpoint      // Spend of output
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_spend(const output_point& outpoint,
        fetch_handler_spend handle_fetch) = 0;

    /**
     * Fetches history list for a Bitcoin address of output or input data,
     * with the block heights and output values associated.
     * The returned history is a list of rows with the following fields:
     *
     * @code
     *  struct history_row
     *  {
     *      point_ident id;
     *      output_point point;
     *      uint64_t height;
     *      union
     *      {
     *          uint64_t value;
     *          uint64_t previous_checksum;
     *      };
     *  };
     * @endcode
     *
     * If id == point_ident::credit, then the row is an output,
     * and the value field in the union is valid.
     * Else if id == point_ident::debit, then use previous_checksum
     * to match the spend with the previous output.
     *
     * To match up the spend row with the output, recompute the
     * checksum from the output row with:
     * @code
     *  spend_checksum(row.point)
     * @endcode
     *
     * Summing the list of values for unspent outpoints gives the balance
     * for an address.
     *
     * @param[in]   address         Bitcoin address
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,              // Status of operation
     *      const history_list& history // History
     *  );
     * @endcode
     * @param[in]   limit           Limit number of returned entries.
     * @param[in]   from_height     Starting block height for history.
     *                              Useful to filter entries or to fetch
     *                              the history in chunks.
     */
    BCB_API virtual void fetch_history(const payment_address& address,
        fetch_handler_history handle_fetch, const uint64_t limit=0,
        const uint64_t from_height=0) = 0;

    /**
     * Fetch possible stealth results. These results can then be iterated
     * to discover new payments belonging to a particular stealth address.
     * This is for recipient privacy.
     * The result is a list of rows with the following fields:
     *
     * @code
     *  struct stealth_row
     *  {
     *      data_chunk ephemkey;
     *      payment_address address;
     *      hash_digest transaction_hash;
     *  };
     * @endcode
     *
     * The prefix is a special value that can be adjusted to provide
     * greater precision at the expense of deniability.
     *
     * from_height is guarantees to return results from that height.
     * It is provided as an optimisation.
     *
     * @param[in]   prefix          Stealth prefix information.
     * @param[in]   handle_fetch    Completion handler for fetch operation.
     * @param[in]   from_height     Starting block height for stealth results.
     *
     * @code
     *  void handle_fetch(
     *      const std::error_code& ec,                   // Status of operation
     *      const blockchain::stealth_list& stealth_rows // Stealth result rows
     *  );
     * @endcode
     */
    BCB_API virtual void fetch_stealth(const binary_type& prefix,
        fetch_handler_stealth handle_fetch, uint64_t from_height=0) = 0;

    /**
     * Be notified of the next blockchain change.
     *
     * Subscriber is notified exactly once of changes to the blockchain
     * and needs to re-subscribe to continue being notified.
     *
     * @param[in]   handle_reorganize   Notification handler for changes
     * @code
     *  void handle_reorganize(
     *      const std::error_code& ec,   // Status of operation
     *      uint64_t fork_point,         // Index where blockchain forks
     *      const block_list& added,     // New blocks added to blockchain
     *      const block_list& removed    // Blocks removed (empty if none)
     *  );
     * @endcode
     */
    BCB_API virtual void subscribe_reorganize(
        reorganize_handler handle_reorganize) = 0;

    // .stop()
    // .shutdown() // close all file descriptors, called once threadpool has stopped.
};

typedef std::function<void (const std::error_code&, const block_type&)>
    blockchain_fetch_handler_block;

/**
 * Fetch a block by height.
 *
 * If the blockchain reorganises, operation may fail halfway.
 *
 * @param[in]   chain           Blockchain service
 * @param[in]   height          Height of block to fetch.
 * @param[in]   handle_fetch    Completion handler for fetch operation.
 * @code
 *  void handle_fetch(
 *      const std::error_code& ec,  // Status of operation
 *      const block_type& blk       // Block header
 *  );
 * @endcode
 */
BCB_API void fetch_block(blockchain& chain, uint64_t height,
    blockchain_fetch_handler_block handle_fetch);

/**
 * Fetch a block by hash.
 *
 * If the blockchain reorganises, operation may fail halfway.
 *
 * @param[in]   chain           Blockchain service
 * @param[in]   hash            Block hash
 * @param[in]   handle_fetch    Completion handler for fetch operation.
 * @code
 *  void handle_fetch(
 *      const std::error_code& ec,  // Status of operation
 *      const block_type& blk       // Block header
 *  );
 * @endcode
 */
BCB_API void fetch_block(blockchain& chain, const hash_digest& hash,
    blockchain_fetch_handler_block handle_fetch);

typedef std::function<
    void (const std::error_code&, const block_locator_type&)>
        blockchain_fetch_handler_block_locator;

/**
 * Creates a block_locator object used to download the blockchain.
 *
 * @param[in]   handle_fetch    Completion handler for fetch operation.
 * @code
 *  void handle_fetch(
 *      const std::error_code& ec,      // Status of operation
 *      const block_locator_type& loc   // Block locator object
 *  );
 * @endcode
 */
BCB_API void fetch_block_locator(blockchain& chain,
    blockchain_fetch_handler_block_locator handle_fetch);

} // namespace chain
} // namespace libbitcoin

#endif

