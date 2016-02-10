#include <iostream>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;
using namespace bc::database;

void show_usage()
{
    std::cerr << "Usage: mmr_create KEY_SIZE VALUE_SIZE "
        "MAP_FILENAME ROWS_FILENAME [BUCKETS]" << std::endl;
}

template <size_t KeySize>
void mmr_create(const size_t value_size,
    const std::string& map_filename, const std::string& rows_filename,
    const index_type buckets)
{
    const auto header_fsize = htdb_record_header_fsize(buckets);

    data_base::touch_file(map_filename);
    mmfile ht_file(map_filename);
    BITCOIN_ASSERT(ht_file.data());
    ht_file.resize(header_fsize + min_records_fsize);

    htdb_record_header header(ht_file, 0);
    header.create(buckets);
    header.start();

    typedef byte_array<KeySize> hash_type;
    const size_t record_fsize = map_record_fsize_multimap<hash_type>();
    BITCOIN_ASSERT(record_fsize == KeySize + 4 + 4);
    const position_type records_start = header_fsize;

    record_allocator alloc(ht_file, records_start, record_fsize);
    alloc.create();
    alloc.start();

    htdb_record<hash_type> ht(header, alloc, "test");

    data_base::touch_file(rows_filename);
    mmfile lrs_file(rows_filename);
    BITCOIN_ASSERT(lrs_file.data());
    lrs_file.resize(min_records_fsize);
    const size_t lrs_record_size = linked_record_offset + value_size;
    record_allocator recs(lrs_file, 0, lrs_record_size);
    recs.create();

    recs.start();
    linked_records lrs(recs);

    multimap_records<hash_type> multimap(ht, lrs, "test");
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
    index_type buckets = 100;
    if (argc == 6)
        buckets = boost::lexical_cast<index_type>(argv[5]);
    if (key_size == 4)
        mmr_create<4>(value_size, map_filename, rows_filename, buckets);
    else if (key_size == 20)
        mmr_create<20>(value_size, map_filename, rows_filename, buckets);
    else if (key_size == 32)
        mmr_create<32>(value_size, map_filename, rows_filename, buckets);
    return 0;
}

