#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

void show_help()
{
    std::cout << "Usage: txdb COMMAND MAP ROWS [ARGS]" << std::endl;
    std::cout << std::endl;
    std::cout << "The most commonly used txdb commands are:" << std::endl;
    std::cout << "  initialize_new  "
        << "Create a new transaction_database" << std::endl;
    std::cout << "  get             "
        << "Fetch transaction data by height or hash" << std::endl;
    std::cout << "  get_info        "
        << "Fetch transaction info by height or hash" << std::endl;
    std::cout << "  store           "
        << "Store a transaction" << std::endl;
    std::cout << "  help            "
        << "Show help for commands" << std::endl;
}

void show_command_help(const std::string& command)
{
    if (command == "initialize_new")
    {
        std::cout << "Usage: txdb " << command << " MAP ROWS "
            << "" << std::endl;
    }
    else if (command == "get")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "INDEX (or) HASH" << std::endl;
    }
    else if (command == "get_info")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "INDEX (or) HASH" << std::endl;
    }
    else if (command == "store")
    {
        std::cout << "Usage: block_db " << command << " MAP ROWS "
            << "HEIGHT INDEX TXDATA" << std::endl;
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
        std::cerr << "txdb: bad point provided." << std::endl;
        return false;
    }
    const std::string& hex_string = strs[0];
    if (hex_string.size() != 64)
    {
        std::cerr << "txdb: bad point provided." << std::endl;
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
        std::cerr << "txdb: bad point provided." << std::endl;
        return false;
    }
    return true;
}

bool parse_key(short_hash& key, const std::string& arg)
{
    payment_address payaddr;
    if (!payaddr.set_encoded(arg))
    {
        std::cerr << "txdb: bad KEY." << std::endl;
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
        std::cerr << "txdb: bad value provided." << std::endl;
        return false;
    }
    return true;
}

bool begins_with(const std::string& input, const char* value)
{
    std::string match(value);
    return input.compare(0, match.length(), match) == 0;
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
    transaction_database db(map_filename, rows_filename);
    if (command == "initialize_new")
    {
        db.initialize_new();
        return 0;
    }
    else if (begins_with(command, "get"))
    {
        if (args.size() != 1)
        {
            show_command_help(command);
            return -1;
        }
        db.start();
        transaction_result* result = nullptr;
        try
        {
            index_type idx = boost::lexical_cast<index_type>(args[0]);
            result = new transaction_result(db.get(idx));
        }
        catch (const boost::bad_lexical_cast&)
        {
            hash_digest hash = decode_hash(args[0]);
            result = new transaction_result(db.get(hash));
        }
        if (!(*result))
        {
            std::cout << "Not found!" << std::endl;
            delete result;
            return -1;
        }
        if (command == "get_info")
        {
            std::cout << "Height: " << result->height()
                << "    Index: " << result->index() << std::endl;
        }
        else
        {
            const transaction_type tx = result->transaction();
            data_chunk rawtx(satoshi_raw_size(tx));
            satoshi_save(tx, rawtx.begin());
            std::cout << rawtx << std::endl;
        }
        delete result;
    }
    else if (command == "store")
    {
        if (args.size() != 3)
        {
            show_command_help(command);
            return -1;
        }
        transaction_metainfo info;
        if (!parse_uint(info.height, args[0]))
            return -1;
        if (!parse_uint(info.index, args[1]))
            return -1;
        data_chunk data = decode_hex(args[2]);
        transaction_type tx;
        satoshi_load(data.begin(), data.end(), tx);
        db.start();
        const index_type idx = db.store(info, tx);
        db.sync();
        std::cout << "Stored: " << idx << std::endl;
    }
    else
    {
        std::cout << "txdb: '" << command
            << "' is not a txdb command. "
            << "See 'txdb --help'." << std::endl;
        return -1;
    }
    return 0;
}

