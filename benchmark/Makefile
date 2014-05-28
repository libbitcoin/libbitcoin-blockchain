CXXFLAGS=$(shell pkg-config --cflags libbitcoin_blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin_blockchain)

default: all

hdb_shard_bench.o: hdb_shard_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

hdb_shard_bench: hdb_shard_bench.o
	$(CXX) -o $@ $< $(LIBS)

all: hdb_shard_bench

clean:
	rm -f hdb_shard_bench
	rm -f *.o
