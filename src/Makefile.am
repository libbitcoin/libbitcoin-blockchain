AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../include ${libczmq_CFLAGS}

lib_LTLIBRARIES = libblockchain.la
libblockchain___la_SOURCES = \
    blockchain.cpp \
    organizer.cpp \
	leveldb/leveldb_blockchain.cpp \
	leveldb/leveldb_chain_keeper.cpp \
	leveldb/leveldb_organizer.cpp \
	leveldb/leveldb_validate_block.cpp \
	leveldb/leveldb_common.cpp

