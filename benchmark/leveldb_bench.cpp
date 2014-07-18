#include <bitcoin/bitcoin.hpp>
#include <bitcoin/utility/timed_section.hpp>
#include <leveldb/db.h>
using namespace libbitcoin;

// Compile this with:
// g++ leveldb_bench.cpp $(pkg-config --cflags --libs libbitcoin) -lleveldb
// Differences in performance become more marked as leveldb
// does not scale linearly.

constexpr size_t total_txs = 200000;
constexpr size_t tx_size = 200;
constexpr size_t buckets = 400000;

data_chunk generate_random_bytes(
    std::default_random_engine& engine, size_t size)
{
    data_chunk result(size);
    for (uint8_t& byte: result)
        byte = engine() % std::numeric_limits<uint8_t>::max();
    return result;
}

leveldb::Options create_open_options()
{
    leveldb::Options options;
    // Open LevelDB databases
    const size_t cache_size = 1 << 20;
    // block_cache, filter_policy and comparator must be deleted after use!
    //options.block_cache = leveldb::NewLRUCache(cache_size / 2);
    options.write_buffer_size = cache_size / 4;
    //options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.compression = leveldb::kNoCompression;
    options.max_open_files = 256;
    options.create_if_missing = true;
    return options;
}

void write_data()
{
    leveldb::Options options = create_open_options();
    leveldb::DB* db = nullptr;
    leveldb::Status status = leveldb::DB::Open(options, "leveldb.db", &db);
    BITCOIN_ASSERT(status.ok());
    BITCOIN_ASSERT(db);

    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        data_chunk value = generate_random_bytes(engine, tx_size);
        hash_digest key = bitcoin_hash(value);
        leveldb::Slice key_slice(
            reinterpret_cast<const char*>(key.data()), key.size());
        leveldb::Slice value_slice(
            reinterpret_cast<const char*>(value.data()), value.size());
        db->Put(leveldb::WriteOptions(), key_slice, value_slice);
    }

    //delete options.block_cache;
    //delete options.filter_policy;
    delete db;
}

void validate_data()
{
    leveldb::Options options = create_open_options();
    leveldb::DB* db = nullptr;
    leveldb::Status status = leveldb::DB::Open(options, "leveldb.db", &db);
    BITCOIN_ASSERT(status.ok());
    BITCOIN_ASSERT(db);

    std::default_random_engine engine;
    for (size_t i = 0; i < total_txs; ++i)
    {
        data_chunk value = generate_random_bytes(engine, tx_size);
        hash_digest key = bitcoin_hash(value);
        leveldb::Slice key_slice(
            reinterpret_cast<const char*>(key.data()), key.size());

        std::string read_value;
        leveldb::Status status =
            db->Get(leveldb::ReadOptions(), key_slice, &read_value);
        if (!status.ok())
            log_error() << status.ToString();
        BITCOIN_ASSERT(status.ok());

        const uint8_t* raw_value =
            reinterpret_cast<const uint8_t*>(read_value.data());
        //log_debug() << data_chunk(raw_value, raw_value + read_value.size());

        BITCOIN_ASSERT(std::equal(value.begin(), value.end(), raw_value));
    }

    delete db;
}

void read_data()
{
    leveldb::Options options = create_open_options();
    leveldb::DB* db = nullptr;
    leveldb::Status status = leveldb::DB::Open(options, "leveldb.db", &db);
    BITCOIN_ASSERT(status.ok());
    BITCOIN_ASSERT(db);

    std::ostringstream oss;
    oss << "txs = " << total_txs << " size = " << tx_size << " |  ";

    std::default_random_engine engine;
    std::vector<hash_digest> keys;
    for (size_t i = 0; i < total_txs; ++i)
    {
        data_chunk value = generate_random_bytes(engine, tx_size);
        hash_digest key = bitcoin_hash(value);
        keys.push_back(key);
    }

    {
        timed_section t("ht.get()", oss.str());
        for (const hash_digest& key: keys)
        {
            leveldb::Slice key_slice(
                reinterpret_cast<const char*>(key.data()), key.size());

            std::string read_value;
            leveldb::Status status =
                db->Get(leveldb::ReadOptions(), key_slice, &read_value);
            //BITCOIN_ASSERT(status.ok());
        }
    }

    delete db;
}

void show_usage()
{
    std::cerr << "Usage: leveldb_bench [-w]" << std::endl;
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

