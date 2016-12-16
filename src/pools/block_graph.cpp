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
#include <bitcoin/blockchain/pools/block_graph.hpp>

#include <vector>
#include <string>
#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/tuple/tuple.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace boost;

block_graph::block_graph()
{
    ////// vertex is typed as size_t, presumably vertex_list::size_type.
    ////vertex v0 = add_vertex(graph_);
    ////vertex v1 = add_vertex(graph_);
    ////vertex v2 = add_vertex(graph_);
    ////vertex v3 = add_vertex(graph_);
    ////vertex v4 = add_vertex(graph_);

    ////add_edge(v0, v1, graph_);
    ////add_edge(v1, v2, graph_);
    ////add_edge(v2, v3, graph_);
    ////add_edge(v0, v4, graph_);

    ////// Does not refer to any member of the graph.
    ////vertex sentinel = traits::null_vertex();

    ////indexes vertex_indexes = get(vertex_index, graph_);

    ////traits::vertex_iterator vertex, vertex_end;
    ////traits::out_edge_iterator out, out_end;
    ////traits::in_edge_iterator in, in_end;

    ////for (boost::tie(vertex, vertex_end) = vertices(graph_);
    ////    vertex != vertex_end; ++vertex)
    ////{
    ////    indexes::value_type index = vertex_indexes[*vertex];

    ////    for (boost::tie(out, out_end) = out_edges(*vertex, graph_);
    ////        out != out_end; ++out)
    ////    {
    ////    }

    ////    for (boost::tie(in, in_end) = in_edges(*vertex, graph_);
    ////        in != in_end; ++in)
    ////    {
    ////    }
    ////}
}

// Add a block to the pool.
bool block_graph::add(block_const_ptr block)
{
    return false;
}

} // namespace blockchain
} // namespace libbitcoin
