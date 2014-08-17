#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

void show_usage()
{
    std::cerr << "Usage: mmr_create KEY_SIZE VALUE_SIZE "
        "MAP_FILENAME ROWS_FILENAME [BUCKETS]" << std::endl;
}

template <size_t KeySize>
void mmr_create(const size_t value_size,
    const std::string& map_filename, const std::string& rows_filename,
    const size_t buckets)
{
    const size_t header_size = htdb_record_header_size(buckets);

    touch_file(map_filename);
    mmfile ht_file(map_filename);
    BITCOIN_ASSERT(ht_file.data());
    ht_file.resize(header_size + min_records_size);

    htdb_record_header header(ht_file, 0);
    header.initialize_new(buckets);
    header.start();

    typedef byte_array<KeySize> hash_type;
    const size_t record_size = map_record_size_multimap<hash_type>();
    BITCOIN_ASSERT(record_size == KeySize + 4 + 4);
    const position_type records_start = header_size;

    record_allocator alloc(ht_file, records_start, record_size);
    alloc.initialize_new();
    alloc.start();

    htdb_record<hash_type> ht(header, alloc);

    touch_file(rows_filename);
    mmfile lrs_file(rows_filename);
    BITCOIN_ASSERT(lrs_file.data());
    lrs_file.resize(min_records_size);
    const size_t lrs_record_size = linked_record_offset + value_size;
    record_allocator recs(lrs_file, 0, lrs_record_size);
    recs.initialize_new();

    recs.start();
    linked_records lrs(recs);

    multimap_records<hash_type> multimap(ht, lrs);
}

int main(int argc, char** argv)
{
    if (argc != 5 && argc != 6)
    {
        show_usage();
        return -1;
    }
    const size_t key_size = boost::lexical_cast<size_t>(argv[1]);
    const size_t value_size = boost::lexical_cast<size_t>(argv[2]);
    const std::string map_filename = argv[3];
    const std::string rows_filename = argv[4];
    size_t buckets = 100;
    if (argc == 6)
        buckets = boost::lexical_cast<size_t>(argv[5]);
    if (key_size == 4)
        mmr_create<4>(value_size, map_filename, rows_filename, buckets);
    else if (key_size == 20)
        mmr_create<20>(value_size, map_filename, rows_filename, buckets);
    else if (key_size == 32)
        mmr_create<32>(value_size, map_filename, rows_filename, buckets);
    return 0;
}

