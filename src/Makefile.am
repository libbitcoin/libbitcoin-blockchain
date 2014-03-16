AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../include ${libbitcoin_CFLAGS}

lib_LTLIBRARIES = libblockchain.la
libblockchain_la_SOURCES = \
    blockchain.cpp \
    organizer.cpp \
	leveldb/leveldb_blockchain.cpp \
	leveldb/leveldb_chain_keeper.cpp \
	leveldb/leveldb_organizer.cpp \
	leveldb/leveldb_validate_block.cpp \
	leveldb/leveldb_common.cpp

libblockchain_la_LIBADD = @LDFLAG_LEVELDB@

