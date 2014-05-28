#include <fstream>
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

int main()
{
    create_new("shard");
    mmfile file("shard");
    BITCOIN_ASSERT(file.data());
    hdb_shard_settings settings;
    hdb_shard shard(file, settings);
    return 0;
}

