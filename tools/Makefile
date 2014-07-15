CXXFLAGS=$(shell pkg-config --cflags libbitcoin_blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin_blockchain)

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

all: show_shard show_hsdb_settings show_array show_records show_linked_records read_htdb_slab_value read_htdb_record_value

clean:
	rm -f show_shard show_hsdb_settings show_array show_records show_linked_records read_htdb_slab_value read_htdb_record_value
	rm -f *.o
