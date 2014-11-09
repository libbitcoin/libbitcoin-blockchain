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
    include/bitcoin/blockchain/define.hpp \
    include/bitcoin/blockchain/blockchain.hpp \
    include/bitcoin/blockchain/transaction_pool.hpp \
    include/bitcoin/blockchain/validate.hpp

bitcoin_blockchain_database_includedir = $(includedir)/bitcoin/blockchain/database
bitcoin_blockchain_database_include_HEADERS = \
    include/bitcoin/blockchain/database/types.hpp

#
# libraries
#
lib_LTLIBRARIES = libbitcoin-blockchain.la

libbitcoin_blockchain_la_SOURCES = \
    src/blockchain.cpp \
    src/transaction_pool.cpp \
    src/validate.cpp \
    src/organizer.cpp \
    src/db_interface.cpp \
    src/database/stealth_database.cpp \
    src/database/history_scan_database.cpp \
    src/database/hsdb_shard.cpp \
    src/database/hsdb_settings.cpp \
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
