#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

void show_help()
{
    std::cout << "Usage: block_db COMMAND MAP ROWS [ARGS]" << std::endl;
    std::cout << std::endl;
    std::cout << "The most commonly used block_db commands are:" << std::endl;
    std::cout << "  initialize_new  "
        << "Create a new block_database" << std::endl;
    std::cout << "  get             "
        << "Fetch block by height or hash" << std::endl;
    std::cout << "  store           "
        << "Store a block" << std::endl;
    std::cout << "  unlink          "
        << "Unlink series of blocks from a height" << std::endl;
    std::cout << "  last_height       "
        << "Show last block height in current chain" << std::endl;
    std::cout << "  help            "
        << "Show help for commands" << std::endl;
}

void show_command_help(const std::string& command)
{
    if (command == "initialize_new")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "" << std::endl;
    }
    else if (command == "get")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "HEIGHT (or) HASH" << std::endl;
    }
    else if (command == "store")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "BLOCK_DATA" << std::endl;
    }
    else if (command == "unlink")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "FROM_HEIGHT" << std::endl;
    }
    else if (command == "last_height")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << std::endl;
    }
    else
    {
        std::cout << "No help available for " << command << std::endl;
    }
}

template <typename Point>
bool parse_point(Point& point, const std::string& arg)
{
    std::vector<std::string> strs;
    boost::split(strs, arg, boost::is_any_of(":"));
    if (strs.size() != 2)
    {
        std::cerr << "block_db: bad point provided." << std::endl;
        return false;
    }
    const std::string& hex_string = strs[0];
    if (hex_string.size() != 64)
    {
        std::cerr << "block_db: bad point provided." << std::endl;
        return false;
    }
    point.hash = decode_hash(hex_string);
    const std::string& index_string = strs[1];
    try
    {
        point.index = boost::lexical_cast<uint32_t>(index_string);
    }
    catch (const boost::bad_lexical_cast&)
    {
        std::cerr << "block_db: bad point provided." << std::endl;
        return false;
    }
    return true;
}

bool parse_key(short_hash& key, const std::string& arg)
{
    payment_address payaddr;
    if (!payaddr.set_encoded(arg))
    {
        std::cerr << "block_db: bad KEY." << std::endl;
        return -1;
    }
    key = payaddr.hash();
    return true;
}

template <typename Uint>
bool parse_uint(Uint& value, const std::string& arg)
{
    try
    {
        value = boost::lexical_cast<Uint>(arg);
    }
    catch (const boost::bad_lexical_cast&)
    {
        std::cerr << "block_db: bad value provided." << std::endl;
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    typedef std::vector<std::string> string_list;
    if (argc < 2)
    {
        show_help();
        return -1;
    }
    const std::string command = argv[1];
    if (command == "help" || command == "-h" || command == "--help")
    {
        if (argc == 3)
        {
            show_command_help(argv[2]);
            return 0;
        }
        show_help();
        return 0;
    }
    if (argc < 4)
    {
        show_command_help(command);
        return -1;
    }
    const std::string map_filename = argv[2];
    const std::string rows_filename = argv[3];
    string_list args;
    for (size_t i = 4; i < argc; ++i)
        args.push_back(argv[i]);
    if (command == "initialize_new")
    {
        touch_file(map_filename);
        touch_file(rows_filename);
    }
    block_database db(map_filename, rows_filename);
    if (command == "initialize_new")
    {
        db.initialize_new();
        return 0;
    }
    else if (command == "get")
    {
        if (args.size() != 1)
        {
            show_command_help(command);
            return -1;
        }
        db.start();
        block_result* result = nullptr;
        try
        {
            size_t height = boost::lexical_cast<size_t>(args[0]);
            result = new block_result(db.get(height));
        }
        catch (const boost::bad_lexical_cast&)
        {
            hash_digest hash = decode_hash(args[0]);
            result = new block_result(db.get(hash));
        }
        if (!(*result))
        {
            std::cout << "Not found!" << std::endl;
            delete result;
            return -1;
        }
        const block_header_type blk_header = result->header();
        // Show details.
        std::cout << "hash: " << hash_block_header(blk_header) << std::endl;
        std::cout << "version: " << blk_header.version << std::endl;
        std::cout << "previous_block_hash: "
            << blk_header.previous_block_hash << std::endl;
        std::cout << "merkle: " << blk_header.merkle << std::endl;
        std::cout << "timestamp: " << blk_header.timestamp << std::endl;
        std::cout << "bits: " << blk_header.bits << std::endl;
        std::cout << "nonce: " << blk_header.nonce << std::endl;
        const size_t txs_size = result->transactions_size();
        if (txs_size)
        {
            std::cout << "Transactions:" << std::endl;
            for (size_t i = 0; i < txs_size; ++i)
                std::cout << "  " << result->transaction_hash(i) << std::endl;
        }
        else
        {
            std::cout << "No transactions" << std::endl;
        }
        delete result;
    }
    else if (command == "store")
    {
        if (args.size() != 1)
        {
            show_command_help(command);
            return -1;
        }
        data_chunk data = decode_hex(args[0]);
        if (data.size() < 80)
        {
            std::cerr << "block_db: BLOCK_DATA must be greater than 80 bytes"
                << std::endl;
            return -1;
        }
        block_type block;
        satoshi_load(data.begin(), data.end(), block);
        db.start();
        db.store(block);
        db.sync();
    }
    else if (command == "unlink")
    {
        if (args.size() != 1)
        {
            show_command_help(command);
            return -1;
        }
        size_t from_height = 0;
        if (!parse_uint(from_height, args[0]))
            return -1;
        db.start();
        db.unlink(from_height);
        db.sync();
    }
    else if (command == "last_height")
    {
        db.start();
        const size_t height = db.last_height();
        if (height == block_database::null_height)
        {
            std::cout << "No blocks exist." << std::endl;
            return -2;
        }
        std::cout << db.last_height() << std::endl;
    }
    else
    {
        std::cout << "block_db: '" << command
            << "' is not a block_db command. "
            << "See 'block_db --help'." << std::endl;
        return -1;
    }
    return 0;
}

