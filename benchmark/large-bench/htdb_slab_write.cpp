#include <boost/lexical_cast.hpp>
#include "iterate.hpp"

void show_usage()
{
    std::cerr << "Usage: htdb_slab_write BUCKETS" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    const size_t buckets = boost::lexical_cast<size_t>(argv[1]);

    const size_t total_txs = read_total("values.seqdb");
    // Copied from prepare.cpp
    const size_t max_tx_size = 400;

    BITCOIN_ASSERT(buckets > 0);
    std::cout << "Buckets: " << buckets << std::endl;

    touch_file("htdb_slabs");
    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 8 * buckets + 8 + total_txs * max_tx_size * 2);

    htdb_slab_header header(file, 0);
    header.initialize_new(buckets);
    header.start();

    slab_allocator alloc(file, 4 + 8 * buckets);
    alloc.initialize_new();
    alloc.start();

    htdb_slab<hash_digest> ht(header, alloc);

    size_t number_wrote = 0;
    auto read = [&ht, &number_wrote](const data_chunk& value)
    {
        const hash_digest key = bitcoin_hash(value);
        auto write = [&value](uint8_t* data)
        {
            std::copy(value.begin(), value.end(), data);
        };
        ht.store(key, value.size(), write);
        ++number_wrote;
    };
    iterate_values("values.seqdb", read);

    alloc.sync();
    std::cout << "Wrote " << number_wrote << " values. Done." << std::endl;
    return 0;
}

