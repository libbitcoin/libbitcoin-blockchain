CXXFLAGS=$(shell pkg-config --cflags libbitcoin-blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin-blockchain)

default: all

show_linked_records.o: show_linked_records.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_linked_records: show_linked_records.o
	$(CXX) -o $@ $< $(LIBS)

show_records.o: show_records.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_records: show_records.o
	$(CXX) -o $@ $< $(LIBS)

count_records.o: count_records.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
count_records: count_records.o
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

transaction_db.o: transaction_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
transaction_db: transaction_db.o
	$(CXX) -o $@ $< $(LIBS)

history_db.o: history_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
history_db: history_db.o
	$(CXX) -o $@ $< $(LIBS)

spend_db.o: spend_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
spend_db: spend_db.o
	$(CXX) -o $@ $< $(LIBS)

stealth_db.o: stealth_db.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
stealth_db: stealth_db.o
	$(CXX) -o $@ $< $(LIBS)

initchain.o: initchain.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
initchain: initchain.o
	$(CXX) -o $@ $< $(LIBS)

all: show_array show_records show_linked_records \
    read_htdb_slab_value read_htdb_record_value mmr_create mmr_lookup \
    mmr_add_row mmr_delete_last_row history_db block_db spend_db initchain \
    transaction_db count_records stealth_db

clean:
	rm -f show_array show_records \
        show_linked_records read_htdb_slab_value read_htdb_record_value \
        mmr_create mmr_lookup mmr_add_row mmr_delete_last_row history_db \
        block_db spend_db initchain count_records stealth_db
	rm -f *.o

