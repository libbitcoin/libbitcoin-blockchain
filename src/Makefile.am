AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../include ${libbitcoin_CFLAGS}

lib_LTLIBRARIES = libbitcoin_blockchain.la
libbitcoin_blockchain_la_SOURCES = \
    organizer.cpp \
    database/stealth_database.cpp \
    database/history_scan_database.cpp \
    database/hsdb_shard.cpp \
    database/hsdb_settings.cpp \
    database/slab_allocator.cpp \
    database/record_allocator.cpp \
    database/linked_records.cpp \
    database/utility.cpp \
	impl/blockchain_impl.cpp \
	impl/simple_chain_impl.cpp \
	impl/blockchain_common.cpp \
	impl/organizer_impl.cpp \
	impl/validate_block_impl.cpp

