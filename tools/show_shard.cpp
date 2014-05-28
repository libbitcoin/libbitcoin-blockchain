#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::blockchain;

constexpr size_t block_height_limit = 100;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: show_shard FILENAME" << std::endl;
        return -1;
    }
    const std::string filename = argv[1];
    mmfile file(filename);
    if (!file.data())
    {
        std::cerr << "show_shard: Error opening file." << std::endl;
        return -1;
    }
    auto deserial = make_deserializer(file.data(), file.data() + file.size());
    std::cout << "values:" << std::endl;
    position_type entry_end = deserial.read_8_bytes();
    std::cout << "  [ " << entry_end << " ]" << std::endl;
    BITCOIN_ASSERT(entry_end >= 1 + shard_max_entries * 8);
    BITCOIN_ASSERT(block_height_limit < shard_max_entries);
    std::cout << "positions:" << std::endl;
    for (size_t i = 0; i < shard_max_entries; ++i)
    {
        position_type entry_position = deserial.read_8_bytes();
        // Don't display all entries... Too many.
        if (i >= block_height_limit)
            continue;
        std::cout << "  " << i << ": [ "
            << entry_position << " ]" << std::endl;
    }
    std::cout << "main_table:" << std::endl;
    position_type file_end = deserial.iterator() - file.data();
    std::cout << "@end = " << file_end << std::endl;
    BITCOIN_ASSERT(file_end == entry_end);
    return 0;
}

