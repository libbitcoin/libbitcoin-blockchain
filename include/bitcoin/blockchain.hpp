///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2015 libbitcoin-blockchain developers (see COPYING).
//
//        GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
//
///////////////////////////////////////////////////////////////////////////////
#ifndef LIBBITCOIN_BLOCKCHAIN_HPP
#define LIBBITCOIN_BLOCKCHAIN_HPP

/**
 * API Users: Include only this header. Direct use of other headers is fragile 
 * and unsupported as header organization is subject to change.
 *
 * Maintainers: Do not include this header internal to this library.
 */

#include <bitcoin/bitcoin.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

#include <bitcoin/blockchain/block.hpp>
#include <bitcoin/blockchain/block_chain.hpp>
#include <bitcoin/blockchain/block_chain_impl.hpp>
#include <bitcoin/blockchain/block_detail.hpp>
#include <bitcoin/blockchain/block_fetcher.hpp>
#include <bitcoin/blockchain/block_info.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/orphan_pool.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/simple_chain.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>
#include <bitcoin/blockchain/transaction_pool_index.hpp>
#include <bitcoin/blockchain/validate_block.hpp>
#include <bitcoin/blockchain/validate_block_impl.hpp>
#include <bitcoin/blockchain/validate_transaction.hpp>
#include <bitcoin/blockchain/version.hpp>
#include <bitcoin/blockchain/database/data_base.hpp>
#include <bitcoin/blockchain/database/database_settings.hpp>
#include <bitcoin/blockchain/database/pointer_array_source.hpp>
#include <bitcoin/blockchain/database/database/block_database.hpp>
#include <bitcoin/blockchain/database/database/history_database.hpp>
#include <bitcoin/blockchain/database/database/spend_database.hpp>
#include <bitcoin/blockchain/database/database/stealth_database.hpp>
#include <bitcoin/blockchain/database/database/transaction_database.hpp>
#include <bitcoin/blockchain/database/disk/disk_array.hpp>
#include <bitcoin/blockchain/database/disk/mmfile.hpp>
#include <bitcoin/blockchain/database/record/htdb_record.hpp>
#include <bitcoin/blockchain/database/record/linked_records.hpp>
#include <bitcoin/blockchain/database/record/multimap_records.hpp>
#include <bitcoin/blockchain/database/record/record_allocator.hpp>
#include <bitcoin/blockchain/database/slab/htdb_slab.hpp>
#include <bitcoin/blockchain/database/slab/slab_allocator.hpp>

#endif
