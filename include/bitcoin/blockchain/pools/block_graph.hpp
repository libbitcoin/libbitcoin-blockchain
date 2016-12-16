/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_BLOCK_GRAPH_HPP
#define LIBBITCOIN_BLOCKCHAIN_BLOCK_GRAPH_HPP

////#include <memory>
#include <string>
#include <string>
#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/tuple/tuple.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe.
class BCB_API block_graph
{
public:
    block_graph();

    /// Add a block to the pool.
    bool add(block_const_ptr block);

private:
    // The graph characteristics.
    typedef boost::vecS out_edge_list;
    typedef boost::vecS vertex_list;
    typedef boost::bidirectionalS direction;
    typedef boost::no_property vertex_properties;
    typedef boost::no_property edge_properties;
    typedef boost::no_property graph_properties;
    typedef boost::listS edge_list;

    // The graph definition.
    typedef boost::adjacency_list<out_edge_list, vertex_list, direction,
        vertex_properties, edge_properties, graph_properties, edge_list> graph;

    // Aliases to graph concepts.
    typedef boost::graph_traits<graph> traits;
    typedef boost::graph_traits<graph>::edge_descriptor edge;
    typedef boost::graph_traits<graph>::vertex_descriptor vertex;
    typedef boost::property_map<graph, boost::vertex_index_t>::type indexes;

    graph graph_;
    mutable upgrade_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
