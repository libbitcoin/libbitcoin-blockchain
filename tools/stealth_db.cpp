#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
using namespace bc;
using namespace bc::chain;

void show_help()
{
    std::cout << "Usage: stealth_db COMMAND INDEX ROWS [ARGS]" << std::endl;
    std::cout << std::endl;
    std::cout << "The most commonly used block_db commands are:" << std::endl;
    std::cout << "  initialize_new  "
        << "Create a new stealth_database" << std::endl;
    std::cout << "  scan            "
        << "Scan entries" << std::endl;
    std::cout << "  store           "
        << "Store a stealth row" << std::endl;
    std::cout << "  unlink          "
        << "Delete all rows after from_height (inclusive)" << std::endl;
    std::cout << "  help            "
        << "Show help for commands" << std::endl;
}

void show_command_help(const std::string& command)
{
    if (command == "initialize_new")
    {
        std::cout << "Usage: stealth_db " << command << " INDEX ROWS "
            << "" << std::endl;
    }
    else if (command == "scan")
    {
        std::cout << "Usage: stealth_db " << command << " INDEX ROWS "
            << "PREFIX FROM_HEIGHT" << std::endl;
    }
    else if (command == "store")
    {
        std::cout << "Usage: stealth_db " << command << " INDEX ROWS "
            << "SCRIPT EPHEMKEY ADDRESS TXHASH" << std::endl;
    }
    else if (command == "unlink")
    {
        std::cout << "Usage: stealth_db " << command << " INDEX ROWS "
            << "FROM_HEIGHT" << std::endl;
    }
    else
    {
        std::cout << "No help available for " << command << std::endl;
    }
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
        std::cerr << "stealth_db: bad value provided." << std::endl;
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
    const std::string index_filename = argv[2];
    const std::string rows_filename = argv[3];
    string_list args;
    for (int i = 4; i < argc; ++i)
        args.push_back(argv[i]);
    if (command == "initialize_new")
    {
        touch_file(index_filename);
        touch_file(rows_filename);
    }
    stealth_database db(index_filename, rows_filename);
    if (command == "initialize_new")
    {
        db.initialize_new();
        return 0;
    }
    else if (command == "scan")
    {
        if (args.size() != 2)
        {
            show_command_help(command);
            return -1;
        }
        db.start();
        std::string prefix_str(args[0]);
        binary_type prefix(prefix_str);
        size_t from_height;
        if (!parse_uint(from_height, args[1]))
            return -1;
        db.start();
        stealth_list rows = db.scan(prefix, from_height);
        for (const auto row: rows)
        {
            std::cout << encode_base16(row.ephemkey) << " "
                << encode_base16(row.address)
                << " " << encode_hash(row.transaction_hash) << std::endl;
        }
    }
    else if (command == "store")
    {
        if (args.size() != 4)
        {
            show_command_help(command);
            return -1;
        }
        // bitfield
        std::string script_str(args[0]);
        script_type script = unpretty(script_str);
        stealth_row row;
        // ephemkey
        if (!decode_base16(row.ephemkey, args[1]))
        {
            std::cerr << "Unable to read ephemeral pubkey." << std::endl;
            return -1;
        }
        // address
        if (!decode_base16(row.address, args[2]))
        {
            std::cerr << "Unable to read address hash." << std::endl;
            return -1;
        }
        // tx hash
        if (!decode_hash(row.transaction_hash, args[3]))
        {
            std::cerr << "Unable to read transaction hash." << std::endl;
            return -1;
        }
        db.start();
        db.store(script, row);
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
    else
    {
        std::cout << "stealth_db: '" << command
            << "' is not a stealth_db command. "
            << "See 'stealth_db --help'." << std::endl;
        return -1;
    }
    return 0;
}

