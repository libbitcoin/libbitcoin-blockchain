#include <fstream>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::blockchain;

void touch_file(const std::string& filename)
{
    std::ofstream outfile(filename);
}

int main()
{
    touch_file("shard");
    mmfile file("shard");
    BITCOIN_ASSERT(file.data());
    hdb_shard_settings settings;
    hdb_shard shard(file, settings);
    //shard.initialize_new();
    size_t total_size = 8 + 8 * shard_max_entries;
    bool success = file.resize(total_size);
    BITCOIN_ASSERT(success);
    return 0;
}

