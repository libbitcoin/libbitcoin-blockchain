CXXFLAGS=$(shell pkg-config --cflags libbitcoin-blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin-blockchain)

default: all

fake.o: fake.cpp step.hpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

fake: fake.o
	$(CXX) -o $@ $< $(LIBS)

all: fake
