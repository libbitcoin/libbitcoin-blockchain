CXXFLAGS=$(shell pkg-config --cflags libbitcoin_blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin_blockchain)

default: all

show_shard.o: show_shard.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

show_shard: show_shard.o
	$(CXX) -o $@ $< $(LIBS)

all: show_shard

clean:
	rm -f show_shard
	rm -f *.o
