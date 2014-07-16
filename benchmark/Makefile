CXXFLAGS=$(shell pkg-config --cflags libbitcoin_blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin_blockchain)

default: all

hsdb_shard_bench.o: hsdb_shard_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
hsdb_shard_bench: hsdb_shard_bench.o
	$(CXX) -o $@ $< $(LIBS)

hsdb_bench.o: hsdb_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
hsdb_bench: hsdb_bench.o
	$(CXX) -o $@ $< $(LIBS)

htdb_record_bench.o: htdb_record_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_record_bench: htdb_record_bench.o
	$(CXX) -o $@ $< $(LIBS)

htdb_slab_bench.o: htdb_slab_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_slab_bench: htdb_slab_bench.o
	$(CXX) -o $@ $< $(LIBS)

all: hsdb_shard_bench hsdb_bench htdb_slab_bench htdb_record_bench

clean:
	rm -f hsdb_shard_bench hsdb_bench htdb_slab_bench htdb_record_bench
	rm -f *.o
