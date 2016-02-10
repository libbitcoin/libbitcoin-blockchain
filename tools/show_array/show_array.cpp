#include <iostream>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;
using namespace bc::database;

template <typename IndexType, typename ValueType>
int show_array(const std::string& filename, position_type offset)
{
    mmfile file(filename);
    if (!file.data())
    {
        std::cerr << "show_array: file failed to open." << std::endl;
        return -1;
    }
    disk_array<IndexType, ValueType> array(file, offset);
    array.start();
    for (index_type i = 0; i < array.size(); ++i)
    {
        auto val = array.read(i);
        std::string val_string = boost::lexical_cast<std::string>(val);
        if (val == array.empty)
            val_string = "";
        std::cout << i << ": " << val_string << std::endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: show_array FILENAME VALUE_SIZE [OFFSET]"
            << std::endl;
        return 0;
    }
    const std::string filename = argv[1];
    const std::string value_size = argv[2];
    position_type offset = 0;
    if (argc == 4)
        offset = boost::lexical_cast<position_type>(argv[3]);
    if (value_size == "4")
        return show_array<uint32_t, uint32_t>(filename, offset);
    else if (value_size == "8")
        return show_array<uint32_t, uint64_t>(filename, offset);
    else
    {
        std::cerr << "show_array: unsupported value size." << std::endl;
        return -1;
    }
    return 0;
}

