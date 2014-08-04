#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>
#include <bitcoin/utility/timed_section.hpp>
using namespace libbitcoin;
using namespace libbitcoin::chain;

constexpr size_t total_txs = 200000;
constexpr size_t key_size = 32 + 4;
constexpr size_t data_size = 32 + 4;

typedef byte_array<data_size> raw_point_type;
constexpr size_t record_size = key_size + 4 + data_size;

template <typename DataBuffer>
void generate_random_bytes(
    std::default_random_engine& engine, DataBuffer& buffer)
{
    for (uint8_t& byte: buffer)
        byte = engine() % std::numeric_limits<uint8_t>::max();
}

void write_data(const size_t buckets)
{
    touch_file("htdb_recs");
    mmfile file("htdb_recs");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 4 * buckets + 4 + total_txs * 36 * 2);

    htdb_record_header header(file, 0);
    header.initialize_new(buckets);
    header.start();

    record_allocator alloc(file, 4 + 4 * buckets, record_size);
    alloc.initialize_new();
    alloc.start();

    htdb_record<raw_point_type> ht(header, alloc);

    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        raw_point_type key, value;
        generate_random_bytes(engine, key);
        generate_random_bytes(engine, value);
        auto write = [&value](uint8_t* data)
        {
            std::copy(value.begin(), value.end(), data);
        };
        ht.store(key, write);
    }

    alloc.sync();
}

void validate_data()
{
    mmfile file("htdb_recs");
    BITCOIN_ASSERT(file.data());

    htdb_record_header header(file, 0);
    header.start();

    record_allocator alloc(file, 4 + 4 * header.size(), record_size);
    alloc.start();

    htdb_record<raw_point_type> ht(header, alloc);

    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        raw_point_type key, value;
        generate_random_bytes(engine, key);
        generate_random_bytes(engine, value);

        const record_type rec = ht.get(key);
        BITCOIN_ASSERT(rec);

        BITCOIN_ASSERT(std::equal(value.begin(), value.end(), rec));
    }
}

void read_data()
{
    mmfile file("htdb_recs");
    BITCOIN_ASSERT(file.data());

    htdb_record_header header(file, 0);
    header.start();

    record_allocator alloc(file, 4 + 4 * header.size(), record_size);
    alloc.start();

    htdb_record<raw_point_type> ht(header, alloc);

    std::ostringstream oss;
    oss << "txs = " << total_txs << " buckets = " << header.size() << " |  ";

    timed_section t("ht.get()", oss.str());
    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        raw_point_type key, value;
        generate_random_bytes(engine, key);
        generate_random_bytes(engine, value);

        const record_type rec = ht.get(key);
    }
}

void show_usage()
{
    std::cerr << "Usage: htdb_record_bench [-w [BUCKETS]]" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc > 4)
    {
        show_usage();
        return -1;
    }
    std::string arg = "";
    if (argc >= 2)
        arg = argv[1];
    std::string buckets_arg = boost::lexical_cast<std::string>(total_txs);
    if (argc >= 3)
        buckets_arg = argv[2];

    if (arg == "-h" || arg == "--help")
    {
        show_usage();
        return 0;
    }
    if (arg == "-w" || arg == "--write")
    {
        const size_t buckets =
            boost::lexical_cast<size_t>(buckets_arg);
        std::cout << "Writing..." << std::endl;
        write_data(buckets);
        std::cout << "Validating..." << std::endl;
        validate_data();
        std::cout << "Done." << std::endl;
    }
    else
    {
        // Perform benchmark.
        read_data();
    }
    return 0;
}

