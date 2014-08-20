CXXFLAGS=$(shell pkg-config --cflags libbitcoin_blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin_blockchain) -lleveldb

default: all

show_shard.o: show_shard.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_shard: show_shard.o
	$(CXX) -o $@ $< $(LIBS)

show_hsdb_settings.o: show_hsdb_settings.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_hsdb_settings: show_hsdb_settings.o
	$(CXX) -o $@ $< $(LIBS)

show_linked_records.o: show_linked_records.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_linked_records: show_linked_records.o
	$(CXX) -o $@ $< $(LIBS)

show_records.o: show_records.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_records: show_records.o
	$(CXX) -o $@ $< $(LIBS)

read_htdb_record_value.o: read_htdb_record_value.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
read_htdb_record_value: read_htdb_record_value.o
	$(CXX) -o $@ $< $(LIBS)

read_htdb_slab_value.o: read_htdb_slab_value.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
read_htdb_slab_value: read_htdb_slab_value.o
	$(CXX) -o $@ $< $(LIBS)

show_array.o: show_array.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_array: show_array.o
	$(CXX) -o $@ $< $(LIBS)

mmr_create.o: mmr_create.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
mmr_create: mmr_create.o
	$(CXX) -o $@ $< $(LIBS)

mmr_lookup.o: mmr_lookup.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
mmr_lookup: mmr_lookup.o
	$(CXX) -o $@ $< $(LIBS)

mmr_add_row.o: mmr_add_row.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
mmr_add_row: mmr_add_row.o
	$(CXX) -o $@ $< $(LIBS)

mmr_delete_last_row.o: mmr_delete_last_row.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
mmr_delete_last_row: mmr_delete_last_row.o
	$(CXX) -o $@ $< $(LIBS)

block_db.o: block_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
block_db: block_db.o
	$(CXX) -o $@ $< $(LIBS)

history_db.o: history_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
history_db: history_db.o
	$(CXX) -o $@ $< $(LIBS)

txdb.o: txdb.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
txdb: txdb.o
	$(CXX) -o $@ $< $(LIBS)

spend_db.o: spend_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
spend_db: spend_db.o
	$(CXX) -o $@ $< $(LIBS)

all: show_shard show_hsdb_settings show_array show_records show_linked_records read_htdb_slab_value read_htdb_record_value mmr_create mmr_lookup mmr_add_row mmr_delete_last_row history_db block_db txdb spend_db

clean:
	rm -f show_shard show_hsdb_settings show_array show_records show_linked_records read_htdb_slab_value read_htdb_record_value mmr_create mmr_lookup mmr_add_row mmr_delete_last_row history_db block_db txdb spend_db
	rm -f *.o
