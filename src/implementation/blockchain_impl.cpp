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
#include <bitcoin/blockchain/implementation/blockchain_impl.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoint.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/implementation/organizer_impl.hpp>
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/settings.hpp>

#define BC_CHAIN_DATABASE_LOCK_FILE "db-lock"

namespace libbitcoin {
namespace blockchain {

using namespace boost::interprocess;
using path = boost::filesystem::path;

static file_lock init_lock(const path& prefix)
{
    // Touch the lock file (open/close).
    const auto lockfile = prefix / BC_CHAIN_DATABASE_LOCK_FILE;
    bc::ofstream file(lockfile.string(), std::ios::app);
    file.close();
    return file_lock(lockfile.string().c_str());
}

blockchain_impl::blockchain_impl(threadpool& pool, const settings& settings)
  : dispatch_(pool),
    flock_(init_lock(settings.database_path)),
    slock_(0),
    stopped_(true),
    store_(settings.database_path),
    database_(store_, settings.history_start_height),
    orphans_(settings.block_pool_capacity),
    chain_(database_),
    organizer_(pool, database_, orphans_, chain_, settings.use_testnet_rules,
        settings.checkpoints)
{
}

void blockchain_impl::start(result_handler handler)
{
    if (!flock_.try_lock())
    {
        handler(error::operation_failed);
        return;
    }

    // TODO: can we actually restart?
    stopped_ = false;

    database_.start();
    handler(error::success);
}

void blockchain_impl::stop()
{
    const auto unhandled = [](const code){};
    stop(unhandled);
}

// TODO: close all file descriptors once threadpool has stopped and return
// result of file close in handler.
void blockchain_impl::stop(result_handler handler)
{
    stopped_ = true;
    organizer_.stop();
    handler(error::success);
}

bool blockchain_impl::stopped()
{
    return stopped_;
}

void blockchain_impl::subscribe_reorganize(reorganize_handler handler)
{
    // Pass this through to the organizer, which issues the notifications.
    organizer_.subscribe_reorganize(handler);
}

void blockchain_impl::start_write()
{
    ++slock_;

    // slock_ must now be odd.
    if (slock_ % 2 == 0)
        throw std::runtime_error("Overlapped write operation.");
}

void blockchain_impl::store(const chain::block& block,
    store_block_handler handler)
{
    dispatch_.ordered(
        std::bind(&blockchain_impl::do_store,
            this, block, handler));
}

void blockchain_impl::do_store(const chain::block& block,
    store_block_handler handler)
{
    if (stopped())
        return;

    start_write();
    const auto stored_detail = std::make_shared<block_detail>(block);

    uint64_t height;
    if (chain_.find_height(height, block.header.hash()))
    {
        const auto info = block_info{ block_status::confirmed, height };
        stop_write(handler, error::duplicate, info);
        return;
    }

    if (!orphans_.add(stored_detail))
    {
        static constexpr uint64_t height = 0;
        const auto info = block_info{ block_status::orphan, height };
        stop_write(handler, error::duplicate, info);
        return;
    }

    organizer_.start();
    stop_write(handler, stored_detail->error(), stored_detail->info());
}

////void blockchain_impl::import(const chain::block& block,
////    block_import_handler handle_import)
////{
////    const auto do_import = [this, block, handle_import]()
////    {
////        start_write();
////        database_.push(block);
////        stop_write(handle_import, error::success);
////    };
////    dispatch_.ordered(do_import);
////}

void blockchain_impl::fetch(perform_read_functor perform_read)
{
    const auto try_read = [this, perform_read]() -> bool
    {
        // Implements the sequential counter logic.
        const size_t slock = slock_;
        return ((slock % 2 == 0) && perform_read(slock));
    };

    const auto do_read = [this, try_read]()
    {
        // Sleeping while waiting for write to complete.
        while (!try_read())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    // Initiate async read operation.
    dispatch_.concurrent(do_read);
}

void blockchain_impl::fetch_block_header(uint64_t height,
    block_header_fetch_handler handler)
{
    const auto do_fetch = [this, height, handler](size_t slock)
    {
        const auto result = database_.blocks.get(height);
        return result ?
            finish_fetch(slock, handler, error::success, result.header()) :
            finish_fetch(slock, handler, error::not_found, chain::header());
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handle_fetch)
{
    const auto do_fetch = [this, hash, handle_fetch](size_t slock)
    {
        const auto handler = handle_fetch;
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.header()) :
            finish_fetch(slock, handler, error::not_found, chain::header());
    };
    fetch(do_fetch);
}

////static hash_list to_hashes(const block_result& result)
////{
////    hash_list hashes;
////    for (size_t index = 0; index < result.transactions_size(); ++index)
////        hashes.push_back(result.transaction_hash(index));
////
////    return hashes;
////}

////void blockchain_impl::fetch_block_transaction_hashes(
////    const hash_digest& hash, transaction_hashes_fetch_handler handler)
////{
////    const auto do_fetch = [this, hash, handler](size_t slock)
////    {
////        const auto result = database_.blocks.get(hash);
////        return result ?
////            finish_fetch(slock, handler, error::success, to_hashes(result)) :
////            finish_fetch(slock, handler, error::not_found, hash_list());
////    };
////    // This was not implemented...
////    //fetch(do_fetch);
////}

void blockchain_impl::fetch_block_height(const hash_digest& hash,
    block_height_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.blocks.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.height()) :
            finish_fetch(slock, handler, error::not_found, 0);
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_last_height(last_height_fetch_handler handler)
{
    const auto do_fetch = [this, handler](size_t slock)
    {
        size_t last_height;
        return database_.blocks.top(last_height) ?
            finish_fetch(slock, handler, error::success, last_height) :
            finish_fetch(slock, handler, error::not_found, 0);
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_transaction(const hash_digest& hash,
    transaction_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.transactions.get(hash);
        const auto tx = result ? result.transaction() : chain::transaction();
        return result ?
            finish_fetch(slock, handler, error::success, tx) :
            finish_fetch(slock, handler, error::not_found, tx);
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_transaction_index(const hash_digest& hash,
    transaction_index_fetch_handler handler)
{
    const auto do_fetch = [this, hash, handler](size_t slock)
    {
        const auto result = database_.transactions.get(hash);
        return result ?
            finish_fetch(slock, handler, error::success, result.height(),
                result.index()) :
            finish_fetch(slock, handler, error::not_found, 0, 0);
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler)
{
    const auto do_fetch = [this, outpoint, handler](size_t slock)
    {
        const auto result = database_.spends.get(outpoint);
        const auto point = result ?
            chain::input_point{ result.hash(), result.index() } :
            chain::input_point();
        return result ?
            finish_fetch(slock, handler, error::success, point) :
            finish_fetch(slock, handler, error::unspent_output, point);
    };

    fetch(do_fetch);
}

void blockchain_impl::fetch_history(const wallet::payment_address& address,
    history_fetch_handler handler, const uint64_t limit,
    const uint64_t from_height)
{
    const auto do_fetch = [this, address, handler, limit, from_height](
        size_t slock)
    {
        const auto history = database_.history.get(address.hash(), limit,
            from_height);
        return finish_fetch(slock, handler, error::success, history);
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_stealth(const binary_type& filter,
    stealth_fetch_handler handler, uint64_t from_height)
{
    const auto do_fetch = [this, filter, handler, from_height](
        size_t slock)
    {
        const auto stealth = database_.stealth.scan(filter, from_height);
        return finish_fetch(slock, handler, error::success, stealth);
    };
    fetch(do_fetch);
}

} // namespace blockchain
} // namespace libbitcoin
