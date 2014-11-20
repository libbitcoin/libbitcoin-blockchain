CXXFLAGS=$(shell pkg-config --cflags libbitcoin-blockchain) -ggdb
LIBS=$(shell pkg-config --libs libbitcoin-blockchain) -lleveldb

default: all

htdb_record_bench.o: htdb_record_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_record_bench: htdb_record_bench.o
	$(CXX) -o $@ $< $(LIBS)

htdb_slab_bench.o: htdb_slab_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_slab_bench: htdb_slab_bench.o
	$(CXX) -o $@ $< $(LIBS)

mmr_bench.o: mmr_bench.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
mmr_bench: mmr_bench.o
	$(CXX) -o $@ $< $(LIBS)

records_access.o: records_access.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
records_access: records_access.o
	$(CXX) -o $@ $< $(LIBS)

drop_caches.o: drop_caches.c
	$(CXX) -o $@ -c $< $(CXXFLAGS)
drop_caches: drop_caches.o
	$(CXX) -o $@ $< $(LIBS)

all: htdb_slab_bench htdb_record_bench mmr_bench records_access drop_caches

clean:
	rm -f htdb_slab_bench htdb_record_bench mmr_bench records_access drop_caches
	rm -f *.o
