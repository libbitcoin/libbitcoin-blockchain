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

#include <bitcoin/database.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/version.hpp>
#include <bitcoin/blockchain/interface/block_chain.hpp>
#include <bitcoin/blockchain/interface/block_fetcher.hpp>
#include <bitcoin/blockchain/interface/full_chain.hpp>
#include <bitcoin/blockchain/interface/simple_chain.hpp>
#include <bitcoin/blockchain/pools/orphan_pool.hpp>
#include <bitcoin/blockchain/pools/orphan_pool_manager.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/pools/transaction_pool_index.hpp>
#include <bitcoin/blockchain/validation/block_validator.hpp>
#include <bitcoin/blockchain/validation/fork.hpp>
#include <bitcoin/blockchain/validation/validate_block.hpp>
#include <bitcoin/blockchain/validation/validate_transaction.hpp>

#endif
