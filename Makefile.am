###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-blockchain developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the m4 subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4


# Distribute, make and install products.
#==============================================================================
# files => ${pkgconfigdir}
#------------------------------------------------------------------------------
pkgconfig_DATA = \
    libbitcoin-blockchain.pc

# files => ${docdir}
#------------------------------------------------------------------------------
doc_DATA = \
    AUTHORS \
    COPYING \
    ChangeLog \
    INSTALL \
    NEWS \
    README

# src/libbitcoin-blockchain.la => ${libdir}
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-blockchain.la
src_libbitcoin_blockchain_la_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_CPPFLAGS} ${bitcoin_consensus_CPPFLAGS}
src_libbitcoin_blockchain_la_LIBADD = ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
src_libbitcoin_blockchain_la_SOURCES = \
    src/settings.cpp \
    src/interface/block_chain.cpp \
    src/interface/block_fetcher.cpp \
    src/pools/orphan_pool.cpp \
    src/pools/orphan_pool_manager.cpp \
    src/pools/transaction_pool.cpp \
    src/pools/transaction_pool_index.cpp \
    src/validation/fork.cpp \
    src/validation/populate_block.cpp \
    src/validation/validate_block.cpp \
    src/validation/validate_input.cpp

# local: test/libbitcoin_blockchain_test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin_blockchain_test_runner.sh

check_PROGRAMS = test/libbitcoin_blockchain_test
test_libbitcoin_blockchain_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_CPPFLAGS} ${bitcoin_consensus_CPPFLAGS}
test_libbitcoin_blockchain_test_LDADD = src/libbitcoin-blockchain.la ${boost_unit_test_framework_LIBS} ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
test_libbitcoin_blockchain_test_SOURCES = \
    test/fork.cpp \
    test/main.cpp \
    test/transaction_pool.cpp \
    test/validate_block.cpp

endif WITH_TESTS

# local: tools/initchain/initchain
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS = tools/initchain/initchain
tools_initchain_initchain_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_CPPFLAGS} ${bitcoin_consensus_CPPFLAGS}
tools_initchain_initchain_LDADD = src/libbitcoin-blockchain.la ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
tools_initchain_initchain_SOURCES = \
    tools/initchain/initchain.cpp

endif WITH_TOOLS

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/blockchain.hpp

include_bitcoin_blockchaindir = ${includedir}/bitcoin/blockchain
include_bitcoin_blockchain_HEADERS = \
    include/bitcoin/blockchain/define.hpp \
    include/bitcoin/blockchain/settings.hpp \
    include/bitcoin/blockchain/version.hpp

include_bitcoin_blockchain_interfacedir = ${includedir}/bitcoin/blockchain/interface
include_bitcoin_blockchain_interface_HEADERS = \
    include/bitcoin/blockchain/interface/block_chain.hpp \
    include/bitcoin/blockchain/interface/block_fetcher.hpp \
    include/bitcoin/blockchain/interface/fast_chain.hpp \
    include/bitcoin/blockchain/interface/safe_chain.hpp

include_bitcoin_blockchain_poolsdir = ${includedir}/bitcoin/blockchain/pools
include_bitcoin_blockchain_pools_HEADERS = \
    include/bitcoin/blockchain/pools/orphan_pool.hpp \
    include/bitcoin/blockchain/pools/orphan_pool_manager.hpp \
    include/bitcoin/blockchain/pools/transaction_pool.hpp \
    include/bitcoin/blockchain/pools/transaction_pool_index.hpp

include_bitcoin_blockchain_validationdir = ${includedir}/bitcoin/blockchain/validation
include_bitcoin_blockchain_validation_HEADERS = \
    include/bitcoin/blockchain/validation/fork.hpp \
    include/bitcoin/blockchain/validation/populate_block.hpp \
    include/bitcoin/blockchain/validation/validate_block.hpp \
    include/bitcoin/blockchain/validation/validate_input.hpp


# Custom make targets.
#==============================================================================
# make target: tools
#------------------------------------------------------------------------------
target_tools = \
    tools/initchain/initchain

tools: ${target_tools}

