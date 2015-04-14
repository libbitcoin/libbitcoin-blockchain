#include <iostream>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/blockchain/database/htdb_record.hpp>
using namespace libbitcoin;
using namespace libbitcoin::blockchain;

template <size_t N>
record_type get_record(htdb_record_header& header, record_allocator& alloc,
    const data_chunk& key_data)
{
    typedef byte_array<N> hash_type;
    htdb_record<hash_type> ht(header, alloc, "test");
    hash_type key;
    BITCOIN_ASSERT(key.size() == key_data.size());
    std::copy(key_data.begin(), key_data.end(), key.begin());
    return ht.get(key);
}

int main(int argc, char** argv)
{
    if (argc != 4 && argc != 5)
    {
        std::cerr
            << "Usage: read_htdb_record_value FILENAME KEY VALUE_SIZE [OFFSET]"
            << std::endl;
        return 0;
    }
    const std::string filename = argv[1];
    const data_chunk key_data = decode_hex(argv[2]);
    const size_t value_size = boost::lexical_cast<size_t>(argv[3]);
    position_type offset = 0;
    if (argc == 5)
        offset = boost::lexical_cast<position_type>(argv[4]);

    mmfile file(filename);
    BITCOIN_ASSERT(file.data());

    htdb_record_header header(file, offset);
    header.start();

    const size_t record_size = key_data.size() + 4 + value_size;
    record_allocator alloc(file, offset + 4 + 4 * header.size(), record_size);
    alloc.start();

    record_type record = nullptr;
    if (key_data.size() == 32)
    {
        record = get_record<32>(header, alloc, key_data);
    }
    else if (key_data.size() == 4)
    {
        record = get_record<4>(header, alloc, key_data);
    }
    else
    {
        std::cerr << "read_htdb_record_value: unsupported key size."
            << std::endl;
        return -1;
    }
    if (!record)
    {
        std::cerr << "read_htdb_record_value: no record found" << std::endl;
        return -2;
    }
    data_chunk data(record, record + value_size);
    std::cout << encode_base16(data) << std::endl;
    return 0;
}

