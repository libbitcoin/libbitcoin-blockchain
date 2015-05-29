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
#include <bitcoin/blockchain/implementation/blockchain_impl.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/checkpoints.hpp>
#include <bitcoin/blockchain/implementation/organizer_impl.hpp>
#include <bitcoin/blockchain/implementation/simple_chain_impl.hpp>

#define BC_CHAIN_DATABASE_LOCK_FILE "db-lock"

namespace libbitcoin {
namespace blockchain {

using namespace boost::interprocess;
using path = boost::filesystem::path;

static organizer_impl organizer_factory(async_strand& reorganize_strand,
    db_interface& database, orphans_pool& orphans, simple_chain& chain,
    blockchain_impl::reorganize_subscriber_type::ptr subscriber,
    const checkpoint& checkpoint)
{
    const auto reorg_handler = [subscriber, &reorganize_strand](
        const std::error_code& code, size_t fork_point, 
        const blockchain::block_list& arrivals,
        const blockchain::block_list& replaced)
    {
        const auto relay = [code, fork_point, arrivals, replaced, subscriber]()
        {
            subscriber->relay(code, fork_point, arrivals, replaced);
        };

        // Post this operation without using the same strand. Therefore calling
        // the reorganize callbacks won't prevent store() from continuing.
        reorganize_strand.queue(relay);
    };

    return organizer_impl(database, orphans, chain, reorg_handler, checkpoint);
}

static file_lock init_lock(const std::string& prefix)
{
    // Touch the lock file (open/close).
    const auto lockfile = path(prefix) / BC_CHAIN_DATABASE_LOCK_FILE;
    bc::ofstream file(lockfile.string(), std::ios::app);
    file.close();
    return file_lock(lockfile.string().c_str());
}

blockchain_impl::blockchain_impl(threadpool& pool, const std::string& prefix,
    const db_active_heights &active_heights, size_t orphan_capacity,
    const checkpoint& checkpoint)
  : ios_(pool.service()),
    write_strand_(pool),
    reorg_strand_(pool),
    flock_(init_lock(prefix)),
    seqlock_(0),
    stopped_(false),
    db_paths_(prefix),
    interface_(db_paths_, active_heights),
    orphans_(orphan_capacity),
    chain_(simple_chain_impl(interface_)),
    reorganize_subscriber_(std::make_shared<reorganize_subscriber_type>(pool)),
    organizer_(organizer_factory(reorg_strand_, interface_, orphans_, chain_,
        reorganize_subscriber_, checkpoint))
{
}
blockchain_impl::~blockchain_impl()
{
}

bool blockchain_impl::start()
{
    if (!flock_.try_lock())
        return false;

    interface_.start();
    return true;
}

bool blockchain_impl::stop()
{
    const uint64_t fork_point = 0;
    reorganize_subscriber_->relay(error::service_stopped, fork_point,
        block_list(), block_list());

    // The relay is an asynchronous call that does not accept a callback.
    // So we cannot currently block or set stopped_ at completion.
    stopped_ = true;
    return stopped_;
}

void blockchain_impl::start_write()
{
    ++seqlock_;

    // seqlock is now odd.
    BITCOIN_ASSERT(seqlock_ % 2 == 1);
}

void blockchain_impl::store(const chain::block& block,
    store_block_handler handle_store)
{
    write_strand_.randomly_queue(
        std::bind(&blockchain_impl::do_store,
            this, block, handle_store));
}
void blockchain_impl::do_store(const chain::block& block,
    store_block_handler handle_store)
{
    // Without this, when we try to stop, blockchain continue
    // to process the queue which might be quite long.
    if (stopped_)
        return;

    start_write();

    const auto stored_detail = std::make_shared<block_detail>(block);
    const auto height = chain_.find_height(block.header.hash());

    if (height != simple_chain::null_height)
    {
        const auto info = block_info{ block_status::confirmed, height };
        stop_write(handle_store, error::duplicate, info);
        return;
    }

    if (!orphans_.add(stored_detail))
    {
        constexpr size_t height = 0;
        const auto info = block_info{ block_status::orphan, height };
        stop_write(handle_store, error::duplicate, info);
        return;
    }

    organizer_.start();
    stop_write(handle_store, stored_detail->error(), stored_detail->info());
}

void blockchain_impl::import(const chain::block& block,
    import_block_handler handle_import)
{
    const auto do_import = [this, block, handle_import]()
    {
        start_write();
        interface_.push(block);
        stop_write(handle_import, bc::error::success);
    };
    write_strand_.randomly_queue(do_import);
}

void blockchain_impl::fetch(perform_read_functor perform_read)
{
    const auto try_read = [this, perform_read]() -> bool
    {
        // Implements the seqlock counter logic.
        size_t slock = seqlock_;
        return ((slock % 2 != 1) && perform_read(slock));
    };

    const auto do_read = [this, try_read]()
    {
        // Sleeping inside seqlock loop is fine since we
        // need to finish write op before we can read anyway.
        while (!try_read())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    // Initiate async read operation.
    ios_.post(do_read);
}

void blockchain_impl::fetch_block_header(uint64_t height,
    fetch_handler_block_header handle_fetch)
{
    const auto do_fetch = [this, height, handle_fetch](size_t slock)
    {
        const auto result = interface_.blocks.get(height);
        if (!result)
        {
            return finish_fetch(slock, handle_fetch,
                error::not_found, chain::block_header());
        }

        return finish_fetch(slock, handle_fetch,
            bc::error::success, result.header());
    };

    fetch(do_fetch);
}

void blockchain_impl::fetch_block_header(const hash_digest& hash,
    fetch_handler_block_header handle_fetch)
{
    const auto do_fetch = [this, hash, handle_fetch](size_t slock)
    {
        const auto result = interface_.blocks.get(hash);
        if (!result)
        {
            return finish_fetch(slock, handle_fetch,
                error::not_found, chain::block_header());
        }

        return finish_fetch(slock, handle_fetch,
            bc::error::success, result.header());
    };

    fetch(do_fetch);
}

void blockchain_impl::fetch_block_transaction_hashes(
    const hash_digest& hash,
    fetch_handler_block_transaction_hashes handle_fetch)
{
    const auto do_fetch = [this, hash, handle_fetch](size_t slock)
    {
        const auto result = interface_.blocks.get(hash);
        if (!result)
            return finish_fetch(slock, handle_fetch,
                error::not_found, hash_list());

        hash_list hashes;
        for (size_t index = 0; index < result.transactions_size(); ++index)
            hashes.push_back(result.transaction_hash(index));

        return finish_fetch(slock, handle_fetch,
            bc::error::success, hashes);
    };
}

void blockchain_impl::fetch_block_height(const hash_digest& hash,
    fetch_handler_block_height handle_fetch)
{
    const auto do_fetch = [this, hash, handle_fetch](size_t slock)
    {
        const auto result = interface_.blocks.get(hash);
        if (!result)
            return finish_fetch(slock, handle_fetch,
                error::not_found, 0);

        return finish_fetch(slock, handle_fetch,
            bc::error::success, result.height());
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_last_height(
    fetch_handler_last_height handle_fetch)
{
    const auto do_fetch = [this, handle_fetch](size_t slock)
    {
        const auto last_height = interface_.blocks.last_height();
        if (last_height == block_database::null_height)
            return finish_fetch(slock, handle_fetch, error::not_found, 0);

        return finish_fetch(slock, handle_fetch,
            bc::error::success, last_height);
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_transaction(
    const hash_digest& hash,
    fetch_handler_transaction handle_fetch)
{
    const auto do_fetch = [this, hash, handle_fetch](size_t slock)
    {
        const auto result = interface_.transactions.get(hash);

        if (!result)
        {
            return finish_fetch(slock, handle_fetch,
                error::not_found, chain::transaction());
        }

        return finish_fetch(slock, handle_fetch,
            bc::error::success, result.transaction());
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_transaction_index(
    const hash_digest& hash,
    fetch_handler_transaction_index handle_fetch)
{
    const auto do_fetch = [this, hash, handle_fetch](size_t slock)
    {
        const auto result = interface_.transactions.get(hash);
        if (!result)
            return finish_fetch(slock, handle_fetch,
                error::not_found, 0, 0);

        return finish_fetch(slock, handle_fetch,
            bc::error::success, result.height(), result.index());
    };
    fetch(do_fetch);
}

void blockchain_impl::fetch_spend(const chain::output_point& outpoint,
    fetch_handler_spend handle_fetch)
{
    const auto do_fetch = [this, outpoint, handle_fetch](size_t slock)
    {
        const auto result = interface_.spends.get(outpoint);

        if (!result)
        {
            return finish_fetch(slock, handle_fetch,
                error::unspent_output, chain::input_point());
        }

        return finish_fetch(slock, handle_fetch, bc::error::success,
            chain::input_point{result.hash(), result.index()});
    };

    fetch(do_fetch);
}

void blockchain_impl::fetch_history(const wallet::payment_address& address,
    fetch_handler_history handle_fetch, const uint64_t limit,
    const uint64_t from_height)
{
    const auto do_fetch = 
        [this, address, handle_fetch, limit, from_height](size_t slock)
    {
        const auto history = interface_.history.get(address.hash(),
            limit, from_height);
        return finish_fetch(slock, handle_fetch, bc::error::success, history);
    };

    fetch(do_fetch);
}

void blockchain_impl::fetch_stealth(const binary_type& prefix,
    fetch_handler_stealth handle_fetch, uint64_t from_height)
{
    const auto do_fetch = 
        [this, prefix, handle_fetch, from_height](size_t slock)
    {
        const auto stealth = interface_.stealth.scan(prefix, from_height);
        return finish_fetch(slock, handle_fetch, bc::error::success, stealth);
    };

    fetch(do_fetch);
}

void blockchain_impl::subscribe_reorganize(
    reorganize_handler handle_reorganize)
{
    reorganize_subscriber_->subscribe(handle_reorganize);
}

} // namespace blockchain
} // namespace libbitcoin
