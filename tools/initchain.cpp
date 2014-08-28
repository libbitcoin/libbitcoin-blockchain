/*
Create a new blockchain database.
*/
#include <bitcoin/blockchain/db_interface.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "initchain: No directory specified." << std::endl;
        return 1;
    }
    const std::string prefix = argv[1];
    db_paths paths(prefix);
    paths.touch_all();
    db_interface interface(paths);
    interface.initialize_new();
    interface.start();
    const block_type genesis = genesis_block();
    interface.push(genesis);
    return 0;
}

