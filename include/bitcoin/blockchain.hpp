///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2020 libbitcoin-blockchain developers (see COPYING).
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

#include <bitcoin/blockchain/block_chain_initializer.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/version.hpp>
#include <bitcoin/blockchain/interface/block_chain.hpp>
#include <bitcoin/blockchain/interface/fast_chain.hpp>
#include <bitcoin/blockchain/interface/safe_chain.hpp>
#include <bitcoin/blockchain/organizers/organize_block.hpp>
#include <bitcoin/blockchain/organizers/organize_header.hpp>
#include <bitcoin/blockchain/organizers/organize_transaction.hpp>
#include <bitcoin/blockchain/pools/block_entry.hpp>
#include <bitcoin/blockchain/pools/block_pool.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/pools/header_entry.hpp>
#include <bitcoin/blockchain/pools/header_pool.hpp>
#include <bitcoin/blockchain/pools/transaction_entry.hpp>
#include <bitcoin/blockchain/pools/transaction_pool.hpp>
#include <bitcoin/blockchain/pools/utilities/anchor_converter.hpp>
#include <bitcoin/blockchain/pools/utilities/child_closure_calculator.hpp>
#include <bitcoin/blockchain/pools/utilities/conflicting_spend_remover.hpp>
#include <bitcoin/blockchain/pools/utilities/parent_closure_calculator.hpp>
#include <bitcoin/blockchain/pools/utilities/priority_calculator.hpp>
#include <bitcoin/blockchain/pools/utilities/stack_evaluator.hpp>
#include <bitcoin/blockchain/pools/utilities/transaction_order_calculator.hpp>
#include <bitcoin/blockchain/pools/utilities/transaction_pool_state.hpp>
#include <bitcoin/blockchain/populate/populate_base.hpp>
#include <bitcoin/blockchain/populate/populate_block.hpp>
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>
#include <bitcoin/blockchain/populate/populate_header.hpp>
#include <bitcoin/blockchain/populate/populate_transaction.hpp>
#include <bitcoin/blockchain/validate/validate_block.hpp>
#include <bitcoin/blockchain/validate/validate_header.hpp>
#include <bitcoin/blockchain/validate/validate_input.hpp>
#include <bitcoin/blockchain/validate/validate_transaction.hpp>

#endif
