#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin/utility/timed_section.hpp>
#include "iterate.hpp"

void show_usage()
{
    std::cerr << "Usage: null_read ITERATIONS" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    const size_t iterations = boost::lexical_cast<size_t>(argv[1]);

    std::ostringstream oss;
    oss << "iterations = " << iterations << " |  ";

    {
        timed_section t("None", oss.str());
        auto read = [](const hash_digest& key)
        {
        };
        randomly_iterate_keys("keys.seqdb", read, iterations);
    }

    return 0;
}

