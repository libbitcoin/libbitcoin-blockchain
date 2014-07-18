#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>
#include <bitcoin/utility/timed_section.hpp>
using namespace libbitcoin;
using namespace libbitcoin::chain;

constexpr size_t total = 20000;
constexpr size_t buckets = 100;

typedef byte_array<32 + 4> raw_point_type;
constexpr size_t record_size = 36 + 4 + 36;

template <typename DataBuffer>
void generate_random_bytes(
    std::default_random_engine& engine, DataBuffer& buffer)
{
    for (uint8_t& byte: buffer)
        byte = engine() % std::numeric_limits<uint8_t>::max();
}

void write_data()
{
    touch_file("htdb_slabs");
    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());
    file.resize(4 + 4 * buckets + 4 + total * 36 * 2);

    htdb_record_header header(file, 0);
    header.initialize_new(buckets);
    header.start();

    record_allocator alloc(file, 4 + 4 * buckets, record_size);
    alloc.initialize_new();
    alloc.start();

    htdb_record<raw_point_type> ht(header, alloc);

    std::default_random_engine engine;
    for (size_t i = 0; i < total; ++i)
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
    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());

    htdb_record_header header(file, 0);
    header.start();

    BITCOIN_ASSERT(header.size() == buckets);

    record_allocator alloc(file, 4 + 4 * header.size(), record_size);
    alloc.start();

    htdb_record<raw_point_type> ht(header, alloc);

    std::default_random_engine engine;
    for (size_t i = 0; i < total; ++i)
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
    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());

    htdb_record_header header(file, 0);
    header.start();

    BITCOIN_ASSERT(header.size() == buckets);

    record_allocator alloc(file, 4 + 4 * header.size(), record_size);
    alloc.start();

    htdb_record<raw_point_type> ht(header, alloc);

    std::ostringstream oss;
    oss << "txs = " << total << " buckets = " << buckets << " |  ";

    timed_section t("ht.get()", oss.str());
    std::default_random_engine engine;
    for (size_t i = 0; i < total; ++i)
    {
        raw_point_type key, value;
        generate_random_bytes(engine, key);
        generate_random_bytes(engine, value);

        const record_type rec = ht.get(key);
    }
}

void show_usage()
{
    std::cerr << "Usage: htdb_bench [-w]" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 1 && argc != 2)
    {
        show_usage();
        return -1;
    }
    const std::string arg = (argc == 2) ? argv[1] : "";
    if (arg == "-h" || arg == "--help")
    {
        show_usage();
        return 0;
    }
    if (arg == "-w" || arg == "--write")
    {
        std::cout << "Writing..." << std::endl;
        write_data();
        std::cout << "Validating..." << std::endl;
        validate_data();
        std::cout << "Done." << std::endl;
    }
    // Perform benchmark.
    read_data();
    return 0;
}

