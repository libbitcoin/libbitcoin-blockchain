#include <leveldb/db.h>
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
    options.create_if_missing = true;
    return options;
}

int main()
{
    leveldb::Options options = create_open_options();
    leveldb::DB* db = nullptr;
    leveldb::Status status = leveldb::DB::Open(options, "leveldb.db", &db);
    BITCOIN_ASSERT(status.ok());
    BITCOIN_ASSERT(db);

    size_t number_wrote = 0;
    auto read = [db, &number_wrote](const data_chunk& value)
    {
        const hash_digest key = bitcoin_hash(value);
        leveldb::Slice key_slice(
            reinterpret_cast<const char*>(key.data()), key.size());
        leveldb::Slice value_slice(
            reinterpret_cast<const char*>(value.data()), value.size());
        db->Put(leveldb::WriteOptions(), key_slice, value_slice);
        ++number_wrote;
    };
    iterate_values("values.seqdb", read);

    delete db;
    std::cout << "Wrote " << number_wrote << " values. Done." << std::endl;
    return 0;
}

