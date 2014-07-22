#include <fstream>
#include <random>
#include <bitcoin/format.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/timed_section.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

void create_new(const std::string& filename)
{
    mmfile file("shard");
    BITCOIN_ASSERT(file.data());
    hsdb_settings settings;
    hsdb_shard shard(file, settings);
    shard.initialize_new();
}

data_chunk generate_random_bytes(
    std::default_random_engine engine, size_t size)
{
    data_chunk result(size);
    for (uint8_t& byte: result)
        byte = engine() % std::numeric_limits<uint8_t>::max();
    return result;
}

void write_random_rows(hsdb_shard& shard,
    hsdb_settings& settings, size_t count)
{
    std::random_device random;

    for (size_t i = 0; i < count; ++i)
    {
        std::default_random_engine engine(random());
        data_chunk key =
            generate_random_bytes(engine, settings.total_key_size);
        data_chunk value =
            generate_random_bytes(engine, settings.row_value_size);

        const size_t scan_bitsize =
            settings.total_key_size * 8 - settings.sharded_bitsize;
        address_bitset scan_key(scan_bitsize);
        boost::from_block_range(key.begin(), key.end(), scan_key);
        const size_t scan_size = (scan_bitsize - 1) / 8 + 1;
        BITCOIN_ASSERT(scan_key.num_blocks() == scan_size);

        shard.add(scan_key, value);
    }
}

void create_db(const std::string& db_name)
{
    create_new(db_name);
    mmfile file(db_name);
    BITCOIN_ASSERT(file.data());
    hsdb_settings settings;
    hsdb_shard shard(file, settings);
    shard.start();

    for (size_t i = 0; i < 1000; ++i)
    {
        if (i % 50 == 0)
            std::cout << "Block " << i << std::endl;
        write_random_rows(shard, settings, 1000);
        shard.sync(i);
    }
}

void scan_test(const std::string& db_name)
{
    mmfile file(db_name);
    BITCOIN_ASSERT(file.data());
    hsdb_settings settings;
    hsdb_shard shard(file, settings);
    shard.start();

    size_t i = 0;
    auto read_row = [&](const uint8_t* row)
    {
        ++i;
    };
    std::string scan = "0111111";
    timed_section t("scan", scan);
    address_bitset key(scan);
    shard.scan(key, read_row, 0);
    std::cout << i << " results" << std::endl;
}

void show_usage()
{
    std::cout << "Usage: hsdb_shard_bench [--init|--benchmark]" << std::endl;
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
        create_db("shard");
    else if (arg == "--benchmark" || arg == "-b")
        scan_test("shard");
    // Delete from block 1 onwards
    //shard.unlink(1);
    return 0;
}

