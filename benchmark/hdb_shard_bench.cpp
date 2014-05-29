#include <fstream>
#include <random>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::blockchain;

void touch_file(const std::string& filename)
{
    std::ofstream outfile(filename);
    // Write byte so file is nonzero size.
    outfile.write("H", 1);
}

void create_new(const std::string& filename)
{
    touch_file("shard");
    mmfile file("shard");
    BITCOIN_ASSERT(file.data());
    hdb_shard_settings settings;
    hdb_shard shard(file, settings);
    shard.initialize_new();
}

data_chunk generate_random_bytes(size_t size)
{
    std::random_device random;
    std::default_random_engine engine(random());
    data_chunk result(size);
    for (uint8_t& byte: result)
        byte = engine() % std::numeric_limits<uint8_t>::max();
    return result;
}

int main()
{
    create_new("shard");
    mmfile file("shard");
    BITCOIN_ASSERT(file.data());
    hdb_shard_settings settings;
    hdb_shard shard(file, settings);
    shard.start();
    // write 10 rows
    for (size_t i = 0; i < 10; ++i)
    {
        data_chunk key = generate_random_bytes(settings.total_key_size);
        data_chunk value = generate_random_bytes(settings.row_value_size);
        const size_t scan_bits =
            settings.total_key_size * 8 - settings.sharded_bits;
        address_bitset scan_key(scan_bits);
        boost::from_block_range(key.begin(), key.end(), scan_key);
        const size_t scan_size = (scan_bits - 1) / 8 + 1;
        BITCOIN_ASSERT(scan_key.num_blocks() == scan_size);
        shard.add(scan_key, value);
    }
    shard.sync(0);
    return 0;
}

