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

htdb_bench.o: htdb_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_bench: htdb_bench.o
	$(CXX) -o $@ $< $(LIBS)

all: hsdb_shard_bench hsdb_bench htdb_bench

clean:
	rm -f hsdb_shard_bench hsdb_bench htdb_bench
	rm -f *.o
