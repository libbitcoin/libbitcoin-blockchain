/*
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

#include <fstream>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <leveldb/cache.h>
#include <leveldb/comparator.h>
#include <leveldb/filter_policy.h>
#include <bitcoin/blockchain/blockchain_impl.hpp>
#include <bitcoin/constants.hpp>
#include <bitcoin/transaction.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/logger.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/format.hpp>
#include "blockchain_common.hpp"
#include "simple_chain_impl.hpp"
#include "organizer_impl.hpp"
#include "fetch_history.hpp"

namespace libbitcoin {
    namespace chain {

using std::placeholders::_1;

class height_comparator
  : public leveldb::Comparator
{
public:
    int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const;

    const char* Name() const;
    void FindShortestSeparator(std::string*, const leveldb::Slice&) const {}
    void FindShortSuccessor(std::string*) const {}
};

int height_comparator::Compare(
    const leveldb::Slice& a, const leveldb::Slice& b) const
{
    uint32_t height_a = recreate_height(a), height_b = recreate_height(b);
    if (height_a < height_b)
        return -1;
    else if (height_a > height_b)
        return +1;
    // a == b
    return 0;
}

const char* height_comparator::Name() const
{
    // Named this way for historical reasons.
    return "depth_comparator";
}

blockchain_impl::blockchain_impl(threadpool& pool)
  : ios_(pool.service()), strand_(pool), reorg_strand_(pool), seqlock_(0)
{
    reorganize_subscriber_ =
        std::make_shared<reorganize_subscriber_type>(pool);
}
blockchain_impl::~blockchain_impl()
{
    delete open_options_.block_cache;
    delete open_options_.filter_policy;
}

void blockchain_impl::start(const std::string& prefix,
    start_handler handle_start)
{
    strand_.randomly_queue(
        [this, prefix, handle_start]
        {
            if (initialize(prefix))
                handle_start(std::error_code());
            else
                handle_start(error::operation_failed);
        });
}

void close(std::unique_ptr<leveldb::DB>& db)
{
    // delete the database, closing it.
    db.reset();
}
void blockchain_impl::stop()
{
    reorganize_subscriber_->relay(error::service_stopped,
        0, block_list(), block_list());
    close(db_block_);
    close(db_block_hash_);
    close(db_tx_);
    close(db_spend_);
    close(db_credit_);
    close(db_debit_);
}

bool open_db(const std::string& prefix, const std::string& db_name,
    std::unique_ptr<leveldb::DB>& db, leveldb::Options open_options)
{
    using boost::filesystem::path;
    path db_path = path(prefix) / db_name;
    leveldb::DB* db_base_ptr = nullptr;

    // LevelDB does not accept UNICODE strings for operating systems that
    // support them, so we must explicity pass an non-UNICODE string for the
    // name and home it works out.
    // Also we must normalize the path name format
    // (e.g. backslashes on Windows).
    // http://www.boost.org/doc/libs/1_46_0/libs/filesystem/v3/doc/reference.html#generic-pathname-format
    // Note: this change should have no effect on a POSIX system.
    leveldb::Status status = leveldb::DB::Open(open_options, 
        db_path.generic_string(), &db_base_ptr);

    if (!status.ok())
    {
        log_fatal(LOG_BLOCKCHAIN) << "Internal error opening '"
            << db_name << "' database: " << status.ToString();
        return false;
    }
    BITCOIN_ASSERT(db_base_ptr);
    // The cointainer ensures db_base_ptr is now managed.
    db.reset(db_base_ptr);
    return true;
}

void open_stealth_db(const std::string& prefix,
    std::unique_ptr<mmfile>& file, std::unique_ptr<stealth_database>& db)
{
    using boost::filesystem::path;
    path db_path = path(prefix) / "stealth.db";
    file.reset(new mmfile(db_path.generic_string().c_str()));
    db.reset(new stealth_database(*file));
}

bool blockchain_impl::initialize(const std::string& prefix)
{
    using boost::filesystem::path;
    // Try to lock the directory first
    path lock_path = path(prefix) / "db-lock";

    // See related comments above
    std::ofstream touch_file(
        lock_path.generic_string(), std::ios::app);

    touch_file.close();

    // See related comments above, and
    // http://stackoverflow.com/questions/11352641/boostfilesystempath-and-fopen
    flock_ = lock_path.generic_string().c_str();

    if (!flock_.try_lock())
    {
        // Database already opened elsewhere
        return false;
    }
    // Create comparator for blocks database.
    height_comparator_.reset(new height_comparator);
    // Open LevelDB databases
    const size_t cache_size = 1 << 20;
    // block_cache, filter_policy and comparator must be deleted after use!
    open_options_.block_cache = leveldb::NewLRUCache(cache_size / 2);
    open_options_.write_buffer_size = cache_size / 4;
    open_options_.filter_policy = leveldb::NewBloomFilterPolicy(10);
    open_options_.compression = leveldb::kNoCompression;
    open_options_.max_open_files = 256;
    open_options_.create_if_missing = true;
    // The blocks database options needs its height comparator too.
    leveldb::Options blocks_open_options = open_options_;
    blocks_open_options.comparator = height_comparator_.get();
    if (!open_db(prefix, "block", db_block_, blocks_open_options))
        return false;
    if (!open_db(prefix, "block_hash", db_block_hash_, open_options_))
        return false;
    if (!open_db(prefix, "tx", db_tx_, open_options_))
        return false;
    if (!open_db(prefix, "spend", db_spend_, open_options_))
        return false;
    if (!open_db(prefix, "credit", db_credit_, open_options_))
        return false;
    if (!open_db(prefix, "debit", db_debit_, open_options_))
        return false;
    // Open custom databases.
    open_stealth_db(prefix, stealth_file_, db_stealth_);
    // Initialize other components.
    leveldb_databases databases{
        db_block_.get(), db_block_hash_.get(), db_tx_.get(),
        db_spend_.get(), db_credit_.get(), db_debit_.get()};
    special_databases special_dbs{
        db_stealth_.get()};
    // G++ has an internal compiler error when you use the implicit * cast.
    common_ = std::make_shared<blockchain_common>(databases, special_dbs);
    // Validate and organisation components.
    orphans_ = std::make_shared<orphans_pool>(20);
    chain_ = std::make_shared<simple_chain_impl>(common_, databases);
    auto reorg_handler = [this](
        const std::error_code& ec, size_t fork_point,
        const blockchain::block_list& arrivals,
        const blockchain::block_list& replaced)
    {
        // Post this operation without using the strand. Therefore
        // calling the reorganize callbacks won't prevent store() from
        // continuing.
        reorg_strand_.queue(
            [=]()
            {
                reorganize_subscriber_->relay(
                    ec, fork_point, arrivals, replaced);
            });
    };
    organize_ = std::make_shared<organizer_impl>(
        common_, orphans_, chain_, reorg_handler);
    return true;
}

void blockchain_impl::begin_write()
{
    ++seqlock_;
    // seqlock is now odd.
    BITCOIN_ASSERT(seqlock_ % 2 == 1);
}

void blockchain_impl::store(const block_type& stored_block,
    store_block_handler handle_store)
{
    strand_.randomly_queue(
        std::bind(&blockchain_impl::do_store,
            this, stored_block, handle_store));
}
void blockchain_impl::do_store(const block_type& stored_block,
    store_block_handler handle_store)
{
    begin_write();
    block_detail_ptr stored_detail =
        std::make_shared<block_detail>(stored_block);
    int height = chain_->find_index(hash_block_header(stored_block.header));
    if (height != -1)
    {
        finish_write(handle_store, error::duplicate,
            block_info{block_status::confirmed, static_cast<size_t>(height)});
        return;
    }
    if (!orphans_->add(stored_detail))
    {
        finish_write(handle_store, error::duplicate,
            block_info{block_status::orphan, 0});
        return;
    }
    organize_->start();
    finish_write(handle_store, stored_detail->errc(), stored_detail->info());
}

void blockchain_impl::import(const block_type& import_block,
    size_t height, import_block_handler handle_import)
{
    strand_.randomly_queue(
        &blockchain_impl::do_import,
            this, import_block, height, handle_import);
}
void blockchain_impl::do_import(const block_type& import_block,
    size_t height, import_block_handler handle_import)
{
    begin_write();
    if (!common_->save_block(height, import_block))
    {
        finish_write(handle_import, error::operation_failed);
        return;
    }
    finish_write(handle_import, std::error_code());
}

void blockchain_impl::fetch(perform_read_functor perform_read)
{
    // Implements the seqlock counter logic.
    auto try_read = [this, perform_read]
        {
            size_t slock = seqlock_;
            if (slock % 2 == 1)
                return false;
            if (perform_read(slock))
                return true;
            return false;
        };
    // Initiate async read operation.
    ios_.post([this, try_read]
        {
            // Sleeping inside seqlock loop is fine since we
            // need to finish write op before we can read anyway.
            while (!try_read())
                std::this_thread::sleep_for(std::chrono::microseconds(100000));
        });
}

void blockchain_impl::fetch_block_header(size_t height,
    fetch_handler_block_header handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::fetch_block_header_by_height,
            this, height, handle_fetch, _1));
}

bool blockchain_impl::fetch_block_header_by_height(size_t height,
    fetch_handler_block_header handle_fetch, size_t slock)
{
    leveldb_block_info blk;
    if (!common_->get_block(blk, height, true, false))
        return finish_fetch(slock, handle_fetch,
            error::not_found, block_header_type());
    return finish_fetch(slock, handle_fetch, std::error_code(), blk.header);
}

void blockchain_impl::fetch_block_header(const hash_digest& block_hash,
    fetch_handler_block_header handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::fetch_block_header_by_hash,
            this, block_hash, handle_fetch, _1));
}

bool blockchain_impl::fetch_block_header_by_hash(
    const hash_digest& block_hash,
    fetch_handler_block_header handle_fetch, size_t slock)
{
    leveldb_block_info blk;
    uint32_t height = common_->get_block_height(block_hash);
    if (height == std::numeric_limits<uint32_t>::max() ||
        !common_->get_block(blk, height, true, false))
    {
        return finish_fetch(slock, handle_fetch,
            error::not_found, block_header_type());
    }
    return finish_fetch(slock, handle_fetch, std::error_code(), blk.header);
}

template<typename Handler, typename SeqLockPtr>
bool fetch_blk_tx_hashes_impl(size_t height,
    blockchain_common_ptr common, Handler handle_fetch,
    size_t slock, SeqLockPtr seqlock)
{
    leveldb_block_info blk;
    if (!common->get_block(blk, height, false, true))
    {
        if (slock != *seqlock)
            return false;
        handle_fetch(error::not_found, hash_digest_list());
        return true;
    }
    if (slock != *seqlock)
        return false;
    handle_fetch(std::error_code(), blk.tx_hashes);
    return true;
}

void blockchain_impl::fetch_block_transaction_hashes(size_t height,
    fetch_handler_block_transaction_hashes handle_fetch)
{
    fetch(
        [this, height, handle_fetch](size_t slock)
        {
            return fetch_blk_tx_hashes_impl(
                height, common_, handle_fetch, slock, &seqlock_);
        });
}

void blockchain_impl::fetch_block_transaction_hashes(
    const hash_digest& block_hash,
    fetch_handler_block_transaction_hashes handle_fetch)
{
    fetch(
        [this, block_hash, handle_fetch](size_t slock)
        {
            uint32_t height = common_->get_block_height(block_hash);
            if (height == std::numeric_limits<uint32_t>::max())
                return false;
            return fetch_blk_tx_hashes_impl(
                height, common_, handle_fetch, slock, &seqlock_);
        });
}

void blockchain_impl::fetch_block_height(const hash_digest& block_hash,
    fetch_handler_block_height handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_block_height,
            this, block_hash, handle_fetch, _1));
}
bool blockchain_impl::do_fetch_block_height(const hash_digest& block_hash,
    fetch_handler_block_height handle_fetch, size_t slock)
{
    uint32_t height = common_->get_block_height(block_hash);
    if (height == std::numeric_limits<uint32_t>::max())
        return finish_fetch(slock, handle_fetch, error::not_found, 0);
    return finish_fetch(slock, handle_fetch, std::error_code(), height);
}

void blockchain_impl::fetch_last_height(
    fetch_handler_last_height handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_last_height,
            this, handle_fetch, _1));
}
bool blockchain_impl::do_fetch_last_height(
    fetch_handler_last_height handle_fetch, size_t slock)
{
    uint32_t last_height = common_->find_last_block_height();
    if (last_height == std::numeric_limits<uint32_t>::max())
        return finish_fetch(slock, handle_fetch, error::not_found, 0);
    return finish_fetch(slock, handle_fetch, std::error_code(), last_height);
}

void blockchain_impl::fetch_transaction(
    const hash_digest& transaction_hash,
    fetch_handler_transaction handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_transaction,
            this, transaction_hash, handle_fetch, _1));
}
bool blockchain_impl::do_fetch_transaction(
    const hash_digest& transaction_hash,
    fetch_handler_transaction handle_fetch, size_t slock)
{
    leveldb_tx_info tx;
    if (!common_->get_transaction(tx, transaction_hash, false, true))
        return finish_fetch(slock, handle_fetch,
            error::not_found, transaction_type());
    return finish_fetch(slock, handle_fetch, std::error_code(), tx.tx);
}

void blockchain_impl::fetch_transaction_index(
    const hash_digest& transaction_hash,
    fetch_handler_transaction_index handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_transaction_index,
            this, transaction_hash, handle_fetch, _1));
}
bool blockchain_impl::do_fetch_transaction_index(
    const hash_digest& transaction_hash,
    fetch_handler_transaction_index handle_fetch, size_t slock)
{
    leveldb_tx_info tx;
    if (!common_->get_transaction(tx, transaction_hash, true, false))
        return finish_fetch(slock, handle_fetch, error::not_found, 0, 0);
    return finish_fetch(slock, handle_fetch, std::error_code(),
        tx.height, tx.index);
}

void blockchain_impl::fetch_spend(const output_point& outpoint,
    fetch_handler_spend handle_fetch)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_spend,
            this, outpoint, handle_fetch, _1));
}
bool blockchain_impl::do_fetch_spend(const output_point& outpoint,
    fetch_handler_spend handle_fetch, size_t slock)
{
    input_point input_spend;
    if (!common_->fetch_spend(outpoint, input_spend))
        return finish_fetch(slock, handle_fetch,
            error::unspent_output, input_point());
    return finish_fetch(slock, handle_fetch, std::error_code(), input_spend);
}

void blockchain_impl::fetch_history(const payment_address& address,
    fetch_handler_history handle_fetch, size_t from_height)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_history,
            this, address, handle_fetch, from_height, _1));
}
bool blockchain_impl::do_fetch_history(const payment_address& address,
    fetch_handler_history handle_fetch, size_t from_height, size_t slock)
{
    auto fetch_history = fetch_history_functor(db_credit_, db_debit_);
    history_list history = fetch_history(address, from_height);
    return finish_fetch(slock, handle_fetch, std::error_code(), history, 0);
}

void blockchain_impl::fetch_stealth(const stealth_prefix& prefix,
    fetch_handler_stealth handle_fetch, size_t from_height)
{
    fetch(
        std::bind(&blockchain_impl::do_fetch_stealth,
            this, prefix, handle_fetch, from_height, _1));
}
bool blockchain_impl::do_fetch_stealth(const stealth_prefix& prefix,
    fetch_handler_stealth handle_fetch, size_t from_height, size_t slock)
{
    stealth_list results;
    auto read_func = [&results, prefix](const uint8_t* it)
    {
        if (!stealth_match(prefix, it))
            return;
        constexpr size_t bitfield_size = sizeof(stealth_bitfield);
        constexpr size_t row_size = bitfield_size + 33 + 21 + 32;
        // Skip bitfield value since we don't need it.
        auto deserial = make_deserializer(it + bitfield_size, it + row_size);
        stealth_row row;
        row.ephemkey = deserial.read_data(33);
        uint8_t address_version = deserial.read_byte();
        const short_hash address_hash = deserial.read_short_hash();
        row.address.set(address_version, address_hash);
        row.transaction_hash = deserial.read_hash();
        BITCOIN_ASSERT(deserial.iterator() == it + row_size);
        results.push_back(row);
    };
    BITCOIN_ASSERT(db_stealth_);
    db_stealth_->scan(read_func, from_height);
    // Finish.
    return finish_fetch(slock, handle_fetch, std::error_code(), results);
}

void blockchain_impl::subscribe_reorganize(
    reorganize_handler handle_reorganize)
{
    reorganize_subscriber_->subscribe(handle_reorganize);
}

    } // namespace chain
} // namespace libbitcoin

