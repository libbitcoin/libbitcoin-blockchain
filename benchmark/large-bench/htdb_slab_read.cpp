#include <boost/lexical_cast.hpp>
#include <bitcoin/utility/timed_section.hpp>
#include "iterate.hpp"

void show_usage()
{
    std::cerr << "Usage: htdb_slab_read ITERATIONS" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    const size_t iterations = boost::lexical_cast<size_t>(argv[1]);

    mmfile file("htdb_slabs");
    BITCOIN_ASSERT(file.data());

    htdb_slab_header header(file, 0);
    header.start();

    slab_allocator alloc(file, 4 + 8 * header.size());
    alloc.start();

    htdb_slab<hash_digest> ht(header, alloc);

    std::ostringstream oss;
    oss << "iterations = " << iterations
        << " buckets = " << header.size() << " |  ";

    {
        timed_section t("ht.get()", oss.str());
        auto read = [&ht](const hash_digest& key)
        {
            const slab_type slab = ht.get(key);
        };
        randomly_iterate_keys("keys.seqdb", read, iterations);
    }

    return 0;
}

