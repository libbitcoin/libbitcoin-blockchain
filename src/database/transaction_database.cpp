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
#include <bitcoin/blockchain/database/transaction_database.hpp>

#include <bitcoin/satoshi_serialize.hpp>
#include <bitcoin/transaction.hpp>
#include <bitcoin/blockchain/database/sizes.hpp>

namespace libbitcoin {
    namespace chain {

constexpr size_t number_buckets = 10000;
constexpr size_t header_size = htdb_slab_header_size(number_buckets);
constexpr size_t initial_file_size = header_size + min_slab_size;

constexpr position_type alloc_offset = header_size;

transaction_database::transaction_database(const std::string& filename)
  : file_(filename), header_(file_, 0),
    allocator_(file_, alloc_offset), map_(header_, allocator_)
{
    BITCOIN_ASSERT(file_.data());
}

void transaction_database::initialize_new()
{
    file_.resize(initial_file_size);
    header_.initialize_new(number_buckets);
    allocator_.initialize_new();
}

void transaction_database::start()
{
    header_.start();
    allocator_.start();
}

void transaction_database::fetch(const position_type position,
    fetch_handler handle_fetch) const
{
    const slab_type slab = allocator_.get(position);
}

void transaction_database::fetch(const hash_digest& hash,
    fetch_handler handle_fetch) const
{
    const slab_type slab = map_.get(hash);
}

void transaction_database::store(const transaction_type& tx)
{
    const hash_digest key = hash_transaction(tx);
    const size_t value_size = satoshi_raw_size(tx);
    auto write = [&tx](uint8_t* data)
    {
        satoshi_save(tx, data);
    };
    map_.store(key, value_size, write);
}

void transaction_database::sync()
{
    allocator_.sync();
}

    } // namespace chain
} // namespace libbitcoin

