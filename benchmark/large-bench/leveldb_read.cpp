#include <boost/lexical_cast.hpp>
#include <leveldb/db.h>
#include <bitcoin/bitcoin/utility/timed_section.hpp>
#include "iterate.hpp"

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
    options.create_if_missing = false;
    return options;
}

void show_usage()
{
    std::cerr << "Usage: leveldb_read ITERATIONS" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    const size_t iterations = boost::lexical_cast<size_t>(argv[1]);

    leveldb::Options options = create_open_options();
    leveldb::DB* db = nullptr;
    leveldb::Status status = leveldb::DB::Open(options, "leveldb.db", &db);
    BITCOIN_ASSERT(status.ok());
    BITCOIN_ASSERT(db);

    std::ostringstream oss;
    oss << "iterations = " << iterations << " |  ";

    {
        timed_section t("leveldb.Get()", oss.str());
        auto read = [db](const hash_digest& key)
        {
            leveldb::Slice key_slice(
                reinterpret_cast<const char*>(key.data()), key.size());

            std::string read_value;
            leveldb::Status status =
                db->Get(leveldb::ReadOptions(), key_slice, &read_value);
        };
        randomly_iterate_keys("keys.seqdb", read, iterations);
    }

    delete db;
    return 0;
}

