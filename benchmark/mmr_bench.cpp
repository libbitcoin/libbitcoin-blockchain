#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/utility/timed_section.hpp>
using namespace libbitcoin;
using namespace libbitcoin::chain;

constexpr size_t total_rows = 2000000;
constexpr size_t key_size = 20;
constexpr size_t value_size = 36 + 4 + 8 + 36 + 4;
constexpr size_t buckets = total_rows;
constexpr size_t min_rows = 1, max_rows = 10;

const std::string map_filename = "mmr_map", rows_filename = "mmr_rows";

data_chunk generate_random_bytes(
    std::default_random_engine& engine, size_t size)
{
    data_chunk result(size);
    for (uint8_t& byte: result)
        byte = engine() % std::numeric_limits<uint8_t>::max();
    return result;
}

void write_data()
{
    BITCOIN_ASSERT(buckets > 0);
    std::cout << "Buckets: " << buckets << std::endl;
    const size_t header_size = htdb_record_header_size(buckets);

    touch_file(map_filename);
    mmfile ht_file(map_filename);
    BITCOIN_ASSERT(ht_file.data());
    ht_file.resize(header_size + min_records_size);

    htdb_record_header header(ht_file, 0);
    header.initialize_new(buckets);
    header.start();

    typedef byte_array<key_size> hash_type;
    const size_t record_size = record_size_htdb<hash_type>(value_size);
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

    std::default_random_engine engine;
    std::uniform_int_distribution<> dist(min_rows, max_rows);

    for (size_t i = 0; i < total_rows; ++i)
    {
        data_chunk value = generate_random_bytes(engine, 20);
        short_hash key = bitcoin_short_hash(value);
        const size_t rows = dist(engine);
        for (size_t j = 0; j < rows; ++j)
        {
            auto write = [](uint8_t* data)
            {
            };
            multimap.add_row(key, write);
        }
    }

    alloc.sync();
    recs.sync();
}

void read_data()
{
    mmfile ht_file(map_filename);
    BITCOIN_ASSERT(ht_file.data());

    htdb_record_header header(ht_file, 0);
    header.start();
    const size_t header_size = htdb_record_header_size(header.size());

    typedef byte_array<key_size> hash_type;
    const size_t record_size = record_size_htdb<hash_type>(value_size);
    const position_type records_start = header_size;

    record_allocator alloc(ht_file, records_start, record_size);
    alloc.start();

    htdb_record<hash_type> ht(header, alloc);

    mmfile lrs_file(rows_filename);
    BITCOIN_ASSERT(lrs_file.data());
    const size_t lrs_record_size = linked_record_offset + value_size;
    record_allocator recs(lrs_file, 0, lrs_record_size);

    recs.start();
    linked_records lrs(recs);

    multimap_records<hash_type> multimap(ht, lrs);

    std::default_random_engine engine;
    std::uniform_int_distribution<> dist(min_rows, max_rows);

    std::ostringstream oss;
    oss << "txs = " << total_rows << " size = " << value_size
        << " buckets = " << header.size() << " |  ";

    std::vector<hash_type> keys;
    for (size_t i = 0; i < total_rows; ++i)
    {
        data_chunk value = generate_random_bytes(engine, 20);
        short_hash key = bitcoin_short_hash(value);
        keys.push_back(key);
        const size_t rows = dist(engine);
        for (size_t j = 0; j < rows; ++j)
        {
            // ...
        }
    }
    std::srand(unsigned(std::time(0)));
    std::random_shuffle(keys.begin(), keys.end());

    timed_section t("multimap.lookup()", oss.str());
    for (const hash_type& key: keys)
    {
        for (const index_type idx: multimap.lookup(key))
        {
        }
    }
}

void show_usage()
{
    std::cout << "Usage: mmr_bench [--init|--benchmark]" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    const std::string arg = argv[1];
    if (arg == "--init" || arg == "-i")
        write_data();
    else if (arg == "--benchmark" || arg == "-b")
        read_data();
    return 0;
}

