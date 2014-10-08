#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: show_hsdb_settings FILENAME" << std::endl;
        return -1;
    }
    const std::string filename = argv[1];
    mmfile file(filename);
    if (!file.data())
    {
        std::cerr << "show_hsdb_settings: Error opening file." << std::endl;
        return -1;
    }
    hsdb_settings settings = load_hsdb_settings(file);
    std::cout << "Version: " << settings.version << std::endl;
    std::cout << "shard_max_entries: "
        << settings.shard_max_entries << std::endl;
    std::cout << "total_key_size: " << settings.total_key_size << std::endl;
    std::cout << "sharded_bitsize: " << settings.sharded_bitsize << std::endl;
    std::cout << "bucket_bitsize: " << settings.bucket_bitsize << std::endl;
    std::cout << "row_value_size: " << settings.row_value_size << std::endl;
    return 0;
}


