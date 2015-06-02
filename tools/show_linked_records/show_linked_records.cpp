/*
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>

// Not published.
#include <bitcoin/blockchain/database/disk_array.hpp>
#include <bitcoin/blockchain/database/linked_records.hpp>
#include <bitcoin/blockchain/database/record_allocator.hpp>

using namespace libbitcoin;
using namespace libbitcoin::blockchain;

struct chain_item
{
    index_type rec_idx;
    data_chunk data;
};

int main(int argc, char** argv)
{
    // test
    uint32_t x = 110;
    uint64_t y = 4;
    if (x == y)
    {
        std::cout << "funny..." << std::endl;
    }
    //
    typedef std::vector<chain_item> chain_type;
    typedef std::vector<chain_type> chain_list;
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: show_records FILENAME RECORD_SIZE [OFFSET]"
            << std::endl;
        return 0;
    }
    const std::string filename = argv[1];
    size_t record_size = boost::lexical_cast<size_t>(argv[2]);
    record_size += linked_record_offset;
    position_type offset = 0;
    if (argc == 4)
        offset = boost::lexical_cast<position_type>(argv[3]);
    mmfile file(filename);
    if (!file.data())
    {
        std::cerr << "show_records: file failed to open." << std::endl;
        return -1;
    }
    record_allocator recs(file, offset, record_size);
    recs.start();
    linked_records lrs(recs);
    chain_list chains;
    for (index_type rec_idx = 0; rec_idx < recs.count(); ++rec_idx)
    {
        BITCOIN_ASSERT(record_size >= 4);
        const record_type rec = recs.get(rec_idx);
        auto deserial = make_deserializer(rec, rec + 4);
        const index_type prev_idx = deserial.read_4_bytes_little_endian();
        const data_chunk data(rec + 4, rec + record_size);
        const chain_item new_item{rec_idx, data};
        if (prev_idx == linked_records::empty)
        {
            // Create new chain
            chain_type chain{{new_item}};
            chains.push_back(chain);
            continue;
        }
        // Iterate all chains, looking for match.
        for (chain_type& chain: chains)
        {
            for (chain_item& item: chain)
            {
                if (item.rec_idx != prev_idx)
                    continue;
                chain.push_back(new_item);
                goto stop_looping;
            }
        }
        BITCOIN_ASSERT_MSG(false, "Internal error or bad file.");
    stop_looping:
        continue;
    }
    // Chains are complete, now display them.
    for (size_t chain_idx = 0; chain_idx < chains.size(); ++chain_idx)
    {
        std::cout << chain_idx << ":" << std::endl;
        chain_type& chain = chains[chain_idx];
        for (size_t item_idx = 0; item_idx < chain.size(); ++item_idx)
        {
            chain_item& item = chain[item_idx];
            std::cout << "  " << item_idx << " (@" << item.rec_idx
                << "): " << encode_base16(item.data) << std::endl;
        }
    }
}

