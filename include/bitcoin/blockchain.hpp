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
#ifndef LIBBITCOIN_BLOCKCHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_HPP

// Publish only interface headers, not implementation.
#include <bitcoin/blockchain/blockchain.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>
#include <bitcoin/blockchain/validate.hpp>
#include <bitcoin/blockchain/database/types.hpp>

#ifdef __linux
    #include <bitcoin/blockchain/database/fsizes.hpp>
    #include <bitcoin/blockchain/database/slab_allocator.hpp>
    #include <bitcoin/blockchain/database/htdb_record.hpp>
    #include <bitcoin/blockchain/database/hsdb_shard.hpp>
    #include <bitcoin/blockchain/database/utility.hpp>
    #include <bitcoin/blockchain/database/multimap_records.hpp>
    #include <bitcoin/blockchain/database/linked_records.hpp>
    #include <bitcoin/blockchain/database/hsdb_settings.hpp>
    #include <bitcoin/blockchain/database/disk_array.hpp>
    #include <bitcoin/blockchain/database/record_allocator.hpp>
    #include <bitcoin/blockchain/database/htdb_slab.hpp>
    #include <bitcoin/blockchain/database/history_scan_database.hpp>
    #include <bitcoin/blockchain/database/history_database.hpp>
    #include <bitcoin/blockchain/database/transaction_database.hpp>
    #include <bitcoin/blockchain/database/spend_database.hpp>
    #include <bitcoin/blockchain/database/block_database.hpp>
    #include <bitcoin/blockchain/database/stealth_database.hpp>
    #include <bitcoin/blockchain/database/mmfile.hpp>
    #include <bitcoin/blockchain/organizer.hpp>
    #include <bitcoin/blockchain/blockchain_impl.hpp>
#endif

#endif
