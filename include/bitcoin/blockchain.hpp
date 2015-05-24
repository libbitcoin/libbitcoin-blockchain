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

#include <bitcoin/blockchain/blockchain.hpp>
#include <bitcoin/blockchain/blockchain_impl.hpp>
#include <bitcoin/blockchain/checkpoints.hpp>
#include <bitcoin/blockchain/db_interface.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/organizer.hpp>
#include <bitcoin/blockchain/organizer_impl.hpp>
#include <bitcoin/blockchain/simple_chain_impl.hpp>
#include <bitcoin/blockchain/transaction_pool.hpp>
#include <bitcoin/blockchain/validate.hpp>
#include <bitcoin/blockchain/validate_block_impl.hpp>
#include <bitcoin/blockchain/version.hpp>
#include <bitcoin/blockchain/database/block_database.hpp>
#include <bitcoin/blockchain/database/disk_array.hpp>
#include <bitcoin/blockchain/database/history_database.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>
#include <bitcoin/blockchain/database/htdb_slab.hpp>
#include <bitcoin/blockchain/database/linked_records.hpp>
#include <bitcoin/blockchain/database/mmfile.hpp>
#include <bitcoin/blockchain/database/multimap_records.hpp>
#include <bitcoin/blockchain/database/record_allocator.hpp>
#include <bitcoin/blockchain/database/slab_allocator.hpp>
#include <bitcoin/blockchain/database/spend_database.hpp>
#include <bitcoin/blockchain/database/stealth_database.hpp>
#include <bitcoin/blockchain/database/transaction_database.hpp>

#endif
