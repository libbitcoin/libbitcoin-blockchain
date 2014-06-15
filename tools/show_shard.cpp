#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/mmfile.hpp>
#include <bitcoin/utility/serializer.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

constexpr size_t block_height_limit = 4000;

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
    // Use default settings.
    hdb_shard_settings settings;
    auto deserial = make_deserializer(file.data(), file.data() + file.size());
    std::cout << "values:" << std::endl;
    position_type entry_end = deserial.read_8_bytes();
    // last_value + 2 + 2 * 256 + rows * (19 + 49)
    std::cout << "  [ " << entry_end << " ]" << std::endl;
    BITCOIN_ASSERT(entry_end >= 1 + shard_max_entries * 8);
    BITCOIN_ASSERT(block_height_limit <= shard_max_entries);
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
    if (block_height_limit < shard_max_entries)
        std::cout << "   ..." << std::endl;
    std::cout << "main_table:" << std::endl;
    while (true)
    {
        position_type start_position = deserial.iterator() - file.data();
        std::cout << "Entry @ " << start_position << std::endl;
        uint16_t number_rows = deserial.read_2_bytes();
        std::cout << "  number_rows: [ " << number_rows << " ]" << std::endl;
        std::cout << "buckets:" << std::endl;
        const size_t number_buckets = settings.number_buckets();
        for (size_t i = 0; i < number_buckets; ++i)
        {
            address_bitset bucket(settings.bucket_bitsize, i);
            std::string bucket_str;
            boost::to_string(bucket, bucket_str);
            std::reverse(bucket_str.begin(), bucket_str.end());
            uint16_t row_index = deserial.read_2_bytes();
            std::cout << "  " << bucket_str << " (" << i
                << "): [ " << row_index << " ]" << std::endl;
        }
        std::cout << "sorted_rows:" << std::endl;
        for (size_t i = 0; i < number_rows; ++i)
        {
            data_chunk key_data = deserial.read_data(settings.scan_size());
            data_chunk value = deserial.read_data(settings.row_value_size);
            address_bitset key(settings.scan_bitsize());
            boost::from_block_range(key_data.begin(), key_data.end(), key);
            std::cout << "  " << i << std::endl;
            std::cout << "    key: " << key << std::endl;
            key.resize(settings.bucket_bitsize);
            std::cout << "    (prefix only: " << key << ")" << std::endl;
            std::cout << "    val: " << value << std::endl;
        }
        position_type end_position = deserial.iterator() - file.data();
        BITCOIN_ASSERT(end_position <= entry_end);
        if (end_position == entry_end)
            break;
    }
    return 0;
}

