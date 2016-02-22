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
    src/block.cpp \
    src/block_chain_impl.cpp \
    src/block_detail.cpp \
    src/block_fetcher.cpp \
    src/organizer.cpp \
    src/orphan_pool.cpp \
    src/settings.cpp \
    src/transaction_pool.cpp \
    src/transaction_pool_index.cpp \
    src/validate_block.cpp \
    src/validate_block_impl.cpp \
    src/validate_transaction.cpp

# local: test/libbitcoin_blockchain_test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin_blockchain_test_runner.sh

check_PROGRAMS = test/libbitcoin_blockchain_test
test_libbitcoin_blockchain_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_CPPFLAGS} ${bitcoin_consensus_CPPFLAGS}
test_libbitcoin_blockchain_test_LDADD = src/libbitcoin-blockchain.la ${boost_unit_test_framework_LIBS} ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
test_libbitcoin_blockchain_test_SOURCES = \
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
    include/bitcoin/blockchain/block.hpp \
    include/bitcoin/blockchain/block_chain.hpp \
    include/bitcoin/blockchain/block_chain_impl.hpp \
    include/bitcoin/blockchain/block_detail.hpp \
    include/bitcoin/blockchain/block_fetcher.hpp \
    include/bitcoin/blockchain/block_info.hpp \
    include/bitcoin/blockchain/define.hpp \
    include/bitcoin/blockchain/organizer.hpp \
    include/bitcoin/blockchain/orphan_pool.hpp \
    include/bitcoin/blockchain/settings.hpp \
    include/bitcoin/blockchain/simple_chain.hpp \
    include/bitcoin/blockchain/transaction_pool.hpp \
    include/bitcoin/blockchain/transaction_pool_index.hpp \
    include/bitcoin/blockchain/validate_block.hpp \
    include/bitcoin/blockchain/validate_block_impl.hpp \
    include/bitcoin/blockchain/validate_transaction.hpp \
    include/bitcoin/blockchain/version.hpp


# Custom make targets.
#==============================================================================
# make target: tools
#------------------------------------------------------------------------------
target_tools = \
    tools/initchain/initchain

tools: ${target_tools}

