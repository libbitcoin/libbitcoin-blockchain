#
# documentation
#
doc_DATA = README INSTALL COPYING

#
# pkg-config
#
pkgconfig_DATA = libbitcoin-blockchain.pc

# look for macros in the m4 subdirectory
ACLOCAL_AMFLAGS = -I m4

#
# headers
#
bitcoin_includedir = $(includedir)/bitcoin
bitcoin_include_HEADERS = \
    include/bitcoin/blockchain.hpp

bitcoin_blockchain_includedir = $(includedir)/bitcoin/blockchain
bitcoin_blockchain_include_HEADERS = \
    include/bitcoin/blockchain/blockchain.hpp \
    include/bitcoin/blockchain/blockchain_impl.hpp \
    include/bitcoin/blockchain/checkpoints.hpp \
    include/bitcoin/blockchain/db_interface.hpp \
    include/bitcoin/blockchain/define.hpp \
    include/bitcoin/blockchain/transaction_pool.hpp \
    include/bitcoin/blockchain/validate.hpp \
    include/bitcoin/blockchain/organizer.hpp

bitcoin_blockchain_database_includedir = $(includedir)/bitcoin/blockchain/database
bitcoin_blockchain_database_include_HEADERS = \
    include/bitcoin/blockchain/database/block_database.hpp \
    include/bitcoin/blockchain/database/disk_array.hpp \
    include/bitcoin/blockchain/database/fsizes.hpp \
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
    include/bitcoin/blockchain/database/transaction_database.hpp \
    include/bitcoin/blockchain/database/types.hpp \
    include/bitcoin/blockchain/database/utility.hpp

bitcoin_blockchain_impl_includedir = $(includedir)/bitcoin/blockchain/impl
bitcoin_blockchain_impl_include_HEADERS = \
    include/bitcoin/blockchain/impl/disk_array.ipp \
    include/bitcoin/blockchain/impl/htdb_record.ipp \
    include/bitcoin/blockchain/impl/htdb_record_list_item.ipp \
    include/bitcoin/blockchain/impl/htdb_slab.ipp \
    include/bitcoin/blockchain/impl/htdb_slab_list_item.ipp \
    include/bitcoin/blockchain/impl/multimap_records.ipp \
    include/bitcoin/blockchain/impl/utility.ipp

#
# libraries
#
lib_LTLIBRARIES = libbitcoin-blockchain.la

libbitcoin_blockchain_la_SOURCES = \
    src/blockchain.cpp \
    src/checkpoints.cpp \
    src/transaction_pool.cpp \
    src/validate.cpp \
    src/organizer.cpp \
    src/db_interface.cpp \
    src/database/stealth_database.cpp \
    src/database/slab_allocator.cpp \
    src/database/record_allocator.cpp \
    src/database/linked_records.cpp \
    src/database/multimap_records.cpp \
    src/database/utility.cpp \
    src/database/mmfile.cpp \
    src/database/transaction_database.cpp \
    src/database/history_database.cpp \
    src/database/spend_database.cpp \
    src/database/block_database.cpp \
    src/impl/blockchain_impl.cpp \
    src/impl/simple_chain_impl.cpp \
    src/impl/organizer_impl.cpp \
    src/impl/validate_block_impl.cpp

libbitcoin_blockchain_la_CPPFLAGS = \
    -I$(srcdir)/include \
    ${bitcoin_CFLAGS}

#
# tests
#
if WITH_TESTS

TESTS = test/libbitcoin_blockchain_test

check_PROGRAMS = test/libbitcoin_blockchain_test

test_libbitcoin_blockchain_test_CPPFLAGS = \
    -I$(srcdir)/include \
    ${bitcoin_CFLAGS}

test_libbitcoin_blockchain_test_SOURCES = \
    test/databases.cpp \
    test/disk_objs.cpp \
    test/htdb.cpp \
    test/interface.cpp \
    test/main.cpp

test_libbitcoin_blockchain_test_LDADD = \
    libbitcoin-blockchain.la \
    -lleveldb \
    -lboost_unit_test_framework \
    ${bitcoin_LIBS}

endif
