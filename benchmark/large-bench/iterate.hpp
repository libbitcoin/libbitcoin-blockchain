#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

typedef std::function<void (const data_chunk&)> read_value;
typedef std::function<void (const hash_digest&)> read_key;

inline size_t read_total(const std::string& filename)
{
    mmfile file(filename);
    auto deserial = make_deserializer_unsafe(file.data());
    return deserial.read_4_bytes();
}

inline void iterate_values(
    const std::string& filename, read_value read)
{
    mmfile file(filename);
    auto deserial = make_deserializer_unsafe(file.data());
    const size_t total_txs = deserial.read_4_bytes();
    for (size_t i = 0; i < total_txs; ++i)
    {
        const size_t tx_size = deserial.read_4_bytes();
        const data_chunk value = deserial.read_data(tx_size);
        read(value);
    }
}

inline void randomly_iterate_keys(
    const std::string& filename, read_key read, const size_t iterations)
{
    mmfile file(filename);
    auto deserial = make_deserializer_unsafe(file.data());
    const size_t total_txs = deserial.read_4_bytes();

    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution<> dist(0, total_txs - 1);

    for (size_t i = 0; i < iterations; ++i)
    {
        const size_t selected = dist(engine);
        const position_type offset = 4 + selected * 32;
        deserial.set_iterator(file.data() + offset);
        const hash_digest hash = deserial.read_hash();
        read(hash);
    }
}

