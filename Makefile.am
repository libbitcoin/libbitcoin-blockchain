###############################################################################
#  Copyright (c) 2014-2020 libbitcoin-blockchain developers (see COPYING).
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
src_libbitcoin_blockchain_la_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_BUILD_CPPFLAGS} ${bitcoin_consensus_BUILD_CPPFLAGS}
src_libbitcoin_blockchain_la_LIBADD = ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
src_libbitcoin_blockchain_la_SOURCES = \
    src/error.cpp \
    src/settings.cpp

# local: test/libbitcoin-blockchain-test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin-blockchain-test_runner.sh

check_PROGRAMS = test/libbitcoin-blockchain-test
test_libbitcoin_blockchain_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_BUILD_CPPFLAGS} ${bitcoin_consensus_BUILD_CPPFLAGS}
test_libbitcoin_blockchain_test_LDADD = src/libbitcoin-blockchain.la ${boost_unit_test_framework_LIBS} ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
test_libbitcoin_blockchain_test_SOURCES = \
    test/error.cpp \
    test/main.cpp \
    test/settings.cpp \
    test/test.cpp \
    test/test.hpp

endif WITH_TESTS

# local: tools/initchain/initchain
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS = tools/initchain/initchain
tools_initchain_initchain_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_BUILD_CPPFLAGS} ${bitcoin_consensus_BUILD_CPPFLAGS}
tools_initchain_initchain_LDADD = src/libbitcoin-blockchain.la ${bitcoin_database_LIBS} ${bitcoin_consensus_LIBS}
tools_initchain_initchain_SOURCES = \
    tools/initchain/main.cpp

endif WITH_TOOLS

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/blockchain.hpp

include_bitcoin_blockchaindir = ${includedir}/bitcoin/blockchain
include_bitcoin_blockchain_HEADERS = \
    include/bitcoin/blockchain/define.hpp \
    include/bitcoin/blockchain/error.hpp \
    include/bitcoin/blockchain/settings.hpp \
    include/bitcoin/blockchain/version.hpp


# Custom make targets.
#==============================================================================
# make target: tools
#------------------------------------------------------------------------------
target_tools = \
    tools/initchain/initchain

tools: ${target_tools}

