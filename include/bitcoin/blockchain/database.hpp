///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2015 libbitcoin-database developers (see COPYING).
//
//        GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
//
///////////////////////////////////////////////////////////////////////////////
#ifndef LIBBITCOIN_DATABASE_HPP
#define LIBBITCOIN_DATABASE_HPP

/**
 * API Users: Include only this header. Direct use of other headers is fragile 
 * and unsupported as header organization is subject to change.
 *
 * Maintainers: Do not include this header internal to this library.
 */

#include <bitcoin/bitcoin.hpp>

#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/database/data_base.hpp>
#include <bitcoin/blockchain/database/database_settings.hpp>
#include <bitcoin/blockchain/database/history_database.hpp>
#include <bitcoin/blockchain/database/pointer_array_source.hpp>
#include <bitcoin/blockchain/database/spend_database.hpp>
#include <bitcoin/blockchain/database/stealth_database.hpp>
#include <bitcoin/blockchain/database/transaction_database.hpp>
#include <bitcoin/blockchain/database/disk/disk_array.hpp>
#include <bitcoin/blockchain/database/disk/mmfile.hpp>
#include <bitcoin/blockchain/database/record/htdb_record.hpp>
#include <bitcoin/blockchain/database/record/linked_records.hpp>
#include <bitcoin/blockchain/database/record/multimap_records.hpp>
#include <bitcoin/blockchain/database/record/record_allocator.hpp>
#include <bitcoin/blockchain/database/slab/htdb_slab.hpp>
#include <bitcoin/blockchain/database/slab/slab_allocator.hpp>

#endif
