#include <fstream>
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
    hdb_shard_settings settings;
    hdb_shard shard(file, settings);
    return 0;
}

