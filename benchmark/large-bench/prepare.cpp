#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

constexpr size_t min_tx_size = 100, max_tx_size = 400;

data_chunk generate_random_bytes(
    std::default_random_engine& engine, size_t size)
{
    data_chunk result(size);
    for (uint8_t& byte: result)
        byte = engine() % std::numeric_limits<uint8_t>::max();
    return result;
}

void show_usage()
{
    std::cerr << "Usage: prepare TOTAL_TXS" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    const size_t total_txs = boost::lexical_cast<size_t>(argv[1]);

    touch_file("values.seqdb");
    mmfile values_file("values.seqdb");
    const size_t values_max_file_size = total_txs * max_tx_size;
    values_file.resize(values_max_file_size);
    auto serial_values = make_serializer(values_file.data());
    serial_values.write_4_bytes(total_txs);

    touch_file("keys.seqdb");
    mmfile keys_file("keys.seqdb");
    const size_t keys_file_size = 4 + total_txs * 32;
    keys_file.resize(keys_file_size);
    auto serial_keys = make_serializer(keys_file.data());
    serial_keys.write_4_bytes(total_txs);

    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution<> dist(min_tx_size, max_tx_size);

    for (size_t i = 0; i < total_txs; ++i)
    {
        const size_t tx_size = dist(engine);
        const data_chunk value = generate_random_bytes(engine, tx_size);
        const hash_digest key = bitcoin_hash(value);
        BITCOIN_ASSERT(value.size() == tx_size);
        serial_values.write_4_bytes(tx_size);
        serial_values.write_data(value);
        serial_keys.write_hash(key);
    }

    std::cout << "Done." << std::endl;
    return 0;
}

