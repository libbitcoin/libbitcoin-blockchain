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

show_array.o: show_array.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
show_array: show_array.o
	$(CXX) -o $@ $< $(LIBS)

all: show_shard show_hsdb_settings show_array

clean:
	rm -f show_shard show_hsdb_settings show_array
	rm -f *.o
