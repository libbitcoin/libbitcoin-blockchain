###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-blockchain developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Standard declarations.
#==============================================================================
# look for macros in the m4 subdirectory
ACLOCAL_AMFLAGS = -I m4


# Automake products.
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

# src/libbitcoin_blockchain.la => ${libdir}
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin_blockchain.la
src_libbitcoin_blockchain_la_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
src_libbitcoin_blockchain_la_LIBADD = ${bitcoin_LIBS}
src_libbitcoin_blockchain_la_SOURCES = \
    src/blockchain.cpp \
    src/checkpoints.cpp \
    src/db_interface.cpp \
    src/organizer.cpp \
    src/transaction_pool.cpp \
    src/validate.cpp \
    src/database/block_database.cpp \
    src/database/history_database.cpp \
    src/database/linked_records.cpp \
    src/database/mmfile.cpp \
    src/database/multimap_records.cpp \
    src/database/record_allocator.cpp \
    src/database/slab_allocator.cpp \
    src/database/spend_database.cpp \
    src/database/stealth_database.cpp \
    src/database/transaction_database.cpp \
    src/database/mman-win32/mman.c \
    src/impl/blockchain_impl.cpp \
    src/impl/organizer_impl.cpp \
    src/impl/simple_chain_impl.cpp \
    src/impl/validate_block_impl.cpp

# local: test/libbitcoin_blockchain_test
#------------------------------------------------------------------------------
if WITH_TESTS

# Execute 'test/libbitcoin_blockchain_test' as a test.
TESTS = test/libbitcoin_blockchain_test

check_PROGRAMS = test/libbitcoin_blockchain_test
test_libbitcoin_blockchain_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
test_libbitcoin_blockchain_test_LDFLAGS = -L${srcdir}/src
test_libbitcoin_blockchain_test_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain ${boost_unit_test_framework_LIBS}
test_libbitcoin_blockchain_test_SOURCES = \
    test/databases.cpp \
    test/disk_objs.cpp \
    test/htdb.cpp \
    test/interface.cpp \
    test/main.cpp

endif WITH_TESTS

# local: tools/block_db/block_db
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS = tools/block_db/block_db
tools_block_db_block_db_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_block_db_block_db_LDFLAGS = -L${srcdir}/src
tools_block_db_block_db_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_block_db_block_db_SOURCES = \
    tools/block_db/block_db.cpp

endif WITH_TOOLS

# local: tools/count_records/count_records
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/count_records/count_records
tools_count_records_count_records_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_count_records_count_records_LDFLAGS = -L${srcdir}/src
tools_count_records_count_records_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_count_records_count_records_SOURCES = \
    tools/count_records/count_records.cpp

endif WITH_TOOLS

# local: tools/history_db/history_db
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/history_db/history_db
tools_history_db_history_db_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_history_db_history_db_LDFLAGS = -L${srcdir}/src
tools_history_db_history_db_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_history_db_history_db_SOURCES = \
    tools/history_db/history_db.cpp

endif WITH_TOOLS

# local: tools/initchain/initchain
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/initchain/initchain
tools_initchain_initchain_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_initchain_initchain_LDFLAGS = -L${srcdir}/src
tools_initchain_initchain_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_initchain_initchain_SOURCES = \
    tools/initchain/initchain.cpp

endif WITH_TOOLS

# local: tools/mmr_add_row/mmr_add_row
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/mmr_add_row/mmr_add_row
tools_mmr_add_row_mmr_add_row_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_mmr_add_row_mmr_add_row_LDFLAGS = -L${srcdir}/src
tools_mmr_add_row_mmr_add_row_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_mmr_add_row_mmr_add_row_SOURCES = \
    tools/mmr_add_row/mmr_add_row.cpp

endif WITH_TOOLS

# local: tools/mmr_create/mmr_create
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/mmr_create/mmr_create
tools_mmr_create_mmr_create_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_mmr_create_mmr_create_LDFLAGS = -L${srcdir}/src
tools_mmr_create_mmr_create_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_mmr_create_mmr_create_SOURCES = \
    tools/mmr_create/mmr_create.cpp

endif WITH_TOOLS

# local: tools/mmr_delete_last_row/mmr_delete_last_row
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/mmr_delete_last_row/mmr_delete_last_row
tools_mmr_delete_last_row_mmr_delete_last_row_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_mmr_delete_last_row_mmr_delete_last_row_LDFLAGS = -L${srcdir}/src
tools_mmr_delete_last_row_mmr_delete_last_row_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_mmr_delete_last_row_mmr_delete_last_row_SOURCES = \
    tools/mmr_delete_last_row/mmr_delete_last_row.cpp

endif WITH_TOOLS

# local: tools/mmr_lookup/mmr_lookup
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/mmr_lookup/mmr_lookup
tools_mmr_lookup_mmr_lookup_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_mmr_lookup_mmr_lookup_LDFLAGS = -L${srcdir}/src
tools_mmr_lookup_mmr_lookup_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_mmr_lookup_mmr_lookup_SOURCES = \
    tools/mmr_lookup/mmr_lookup.cpp

endif WITH_TOOLS

# local: tools/read_htdb_record_value/read_htdb_record_value
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/read_htdb_record_value/read_htdb_record_value
tools_read_htdb_record_value_read_htdb_record_value_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_read_htdb_record_value_read_htdb_record_value_LDFLAGS = -L${srcdir}/src
tools_read_htdb_record_value_read_htdb_record_value_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_read_htdb_record_value_read_htdb_record_value_SOURCES = \
    tools/read_htdb_record_value/read_htdb_record_value.cpp

endif WITH_TOOLS

# local: tools/read_htdb_slab_value/read_htdb_slab_value
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/read_htdb_slab_value/read_htdb_slab_value
tools_read_htdb_slab_value_read_htdb_slab_value_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_read_htdb_slab_value_read_htdb_slab_value_LDFLAGS = -L${srcdir}/src
tools_read_htdb_slab_value_read_htdb_slab_value_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_read_htdb_slab_value_read_htdb_slab_value_SOURCES = \
    tools/read_htdb_slab_value/read_htdb_slab_value.cpp

endif WITH_TOOLS

# local: tools/show_array/show_array
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/show_array/show_array
tools_show_array_show_array_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_show_array_show_array_LDFLAGS = -L${srcdir}/src
tools_show_array_show_array_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_show_array_show_array_SOURCES = \
    tools/show_array/show_array.cpp

endif WITH_TOOLS

# local: tools/show_linked_records/show_linked_records
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/show_linked_records/show_linked_records
tools_show_linked_records_show_linked_records_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_show_linked_records_show_linked_records_LDFLAGS = -L${srcdir}/src
tools_show_linked_records_show_linked_records_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_show_linked_records_show_linked_records_SOURCES = \
    tools/show_linked_records/show_linked_records.cpp

endif WITH_TOOLS

# local: tools/show_records/show_records
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/show_records/show_records
tools_show_records_show_records_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_show_records_show_records_LDFLAGS = -L${srcdir}/src
tools_show_records_show_records_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_show_records_show_records_SOURCES = \
    tools/show_records/show_records.cpp

endif WITH_TOOLS

# local: tools/spend_db/spend_db
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/spend_db/spend_db
tools_spend_db_spend_db_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_spend_db_spend_db_LDFLAGS = -L${srcdir}/src
tools_spend_db_spend_db_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_spend_db_spend_db_SOURCES = \
    tools/spend_db/spend_db.cpp

endif WITH_TOOLS

# local: tools/stealth_db/stealth_db
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/stealth_db/stealth_db
tools_stealth_db_stealth_db_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_stealth_db_stealth_db_LDFLAGS = -L${srcdir}/src
tools_stealth_db_stealth_db_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_stealth_db_stealth_db_SOURCES = \
    tools/stealth_db/stealth_db.cpp

endif WITH_TOOLS

# local: tools/transaction_db/transaction_db
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS += tools/transaction_db/transaction_db
tools_transaction_db_transaction_db_CPPFLAGS = -I${srcdir}/include ${bitcoin_CPPFLAGS}
tools_transaction_db_transaction_db_LDFLAGS = -L${srcdir}/src
tools_transaction_db_transaction_db_LDADD = ${bitcoin_LIBS} -lbitcoin_blockchain
tools_transaction_db_transaction_db_SOURCES = \
    tools/transaction_db/transaction_db.cpp

endif WITH_TOOLS

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/blockchain.hpp

include_bitcoin_blockchaindir = ${includedir}/bitcoin/blockchain
include_bitcoin_blockchain_HEADERS = \
    include/bitcoin/blockchain/blockchain.hpp \
    include/bitcoin/blockchain/blockchain_impl.hpp \
    include/bitcoin/blockchain/checkpoints.hpp \
    include/bitcoin/blockchain/db_interface.hpp \
    include/bitcoin/blockchain/define.hpp \
    include/bitcoin/blockchain/organizer.hpp \
    include/bitcoin/blockchain/transaction_pool.hpp \
    include/bitcoin/blockchain/validate.hpp \
    include/bitcoin/blockchain/version.hpp

include_bitcoin_blockchain_databasedir = ${includedir}/bitcoin/blockchain/database
include_bitcoin_blockchain_database_HEADERS = \
    include/bitcoin/blockchain/database/block_database.hpp \
    include/bitcoin/blockchain/database/disk_array.hpp \
    include/bitcoin/blockchain/database/history_database.hpp \
    include/bitcoin/blockchain/database/htdb_record.hpp \
    include/bitcoin/blockchain/database/htdb_slab.hpp \
    include/bitcoin/blockchain/database/linked_records.hpp \
    include/bitcoin/blockchain/database/mmfile.hpp \
    include/bitcoin/blockchain/database/multimap_records.hpp \
    include/bitcoin/blockchain/database/record_allocator.hpp \
    include/bitcoin/blockchain/database/slab_allocator.hpp \
    include/bitcoin/blockchain/database/spend_database.hpp \
    include/bitcoin/blockchain/database/stealth_database.hpp \
    include/bitcoin/blockchain/database/transaction_database.hpp

include_bitcoin_blockchain_impldir = ${includedir}/bitcoin/blockchain/impl
include_bitcoin_blockchain_impl_HEADERS = \
    include/bitcoin/blockchain/impl/disk_array.ipp \
    include/bitcoin/blockchain/impl/htdb_record.ipp \
    include/bitcoin/blockchain/impl/htdb_record_list_item.ipp \
    include/bitcoin/blockchain/impl/htdb_slab.ipp \
    include/bitcoin/blockchain/impl/htdb_slab_list_item.ipp \
    include/bitcoin/blockchain/impl/multimap_records.ipp \
    include/bitcoin/blockchain/impl/remainder.ipp

