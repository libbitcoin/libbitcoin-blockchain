#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/utility/timed_section.hpp>
using namespace bc;
using namespace bc::chain;

constexpr size_t record_size = 400;

void write_data(const size_t count)
{
    touch_file("recs");
    mmfile file("recs");
    BITCOIN_ASSERT(file.data());
    file.resize(min_records_size);
    record_allocator alloc(file, 0, record_size);
    alloc.initialize_new();
    alloc.start();

    for (size_t i = 0; i < count; ++i)
    {
        alloc.allocate();
    }
    alloc.sync();
}

void read_data(const size_t iterations)
{
    mmfile file("recs");
    BITCOIN_ASSERT(file.data());
    record_allocator alloc(file, 0, record_size);
    alloc.start();

    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution<> dist(0, alloc.size() - 1);

    std::ostringstream oss;
    oss << "total = " << alloc.size() << " record_size = " << record_size
        << " iterations = " << iterations << " |  ";
    timed_section t("ht.get()", oss.str());

    // Iterate sequentially...
    /*
    for (size_t i = 0; i < iterations / alloc.size(); ++i)
    {
        for (size_t j = 0; j < alloc.size(); ++j)
            alloc.get(j)[0];
    }
    */

    for (size_t i = 0; i < iterations; ++i)
    {
        const index_type idx = dist(engine);
        alloc.get(idx)[0];
    }
}

int main()
{
    write_data(10000);
    read_data(10000000);
    return 0;
}

