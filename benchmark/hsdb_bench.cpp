#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/timed_section.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

int main(int argc, char** argv)
{
    //create_hsdb("foo");
    //std::cout << "Created." << std::endl;

    data_chunk value(49);
    history_scan_database db("foo");
    std::cout << "Loaded." << std::endl;

    address_bitset key(std::string("1101101001"));
    key.resize(20 * 8);
    db.add(key, value);
    std::cout << "db.sync()" << std::endl;
    db.sync(0);

    auto read_row = [](const uint8_t* row)
    {
        std::cout << "Row..." << std::endl;
    };
    address_bitset scan(std::string("101101001"));
    std::cout << "db.scan(" << scan << ")" << std::endl;
    db.scan(scan, read_row, 0);
    return 0;
}

