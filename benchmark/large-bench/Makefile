CXXFLAGS=$(shell pkg-config --cflags libbitcoin-blockchain)
LIBS=$(shell pkg-config --libs libbitcoin-blockchain)

default: all

prepare.o: prepare.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
prepare: prepare.o
	$(CXX) -o $@ $< $(LIBS)

leveldb_write.o: leveldb_write.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
leveldb_write: leveldb_write.o
	$(CXX) -o $@ $< $(LIBS)

htdb_slab_write.o: htdb_slab_write.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_slab_write: htdb_slab_write.o
	$(CXX) -o $@ $< $(LIBS)

leveldb_read.o: leveldb_read.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
leveldb_read: leveldb_read.o
	$(CXX) -o $@ $< $(LIBS)

htdb_slab_read.o: htdb_slab_read.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
htdb_slab_read: htdb_slab_read.o
	$(CXX) -o $@ $< $(LIBS)

null_read.o: null_read.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)
null_read: null_read.o
	$(CXX) -o $@ $< $(LIBS)

all: htdb_slab_read htdb_slab_write leveldb_read leveldb_write null_read prepare

clean:
	rm -f htdb_slab_read htdb_slab_write leveldb_read leveldb_write null_read prepare
	rm -f *.o
