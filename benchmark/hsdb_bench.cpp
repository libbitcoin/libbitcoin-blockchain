#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

int main(int argc, char** argv)
{
    //create_hsdb("foo");
    //std::cout << "Created." << std::endl;

    history_scan_database db("foo");
    std::cout << "Loaded." << std::endl;

    address_bitset key(std::string("1101101001"));
    key.resize(20 * 8);
    db.add(key,
        point_type{point_ident_type::output,
            output_point{null_hash, 0}},
        110,
        4);
    std::cout << "db.sync()" << std::endl;
    db.sync(0);

    auto read_row = [](
        const point_type& point, uint32_t height, uint64_t value)
    {
        std::cout << "Row... " << value << std::endl;
    };
    address_bitset scan(std::string("101101001"));
    std::cout << "db.scan(" << scan << ")" << std::endl;
    db.scan(scan, read_row, 0);
    return 0;
}

