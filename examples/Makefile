CXXFLAGS=$(shell pkg-config --cflags libbitcoin-blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin-blockchain)

default: all

fullnode.o: fullnode.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
fullnode: fullnode.o
	$(CXX) -o $@ $< $(LIBS)

all: fullnode

clean:
	rm -f fullnode
	rm -f *.o
