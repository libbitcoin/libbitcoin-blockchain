/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_BLOCKCHAIN_HEADER_POOL_HPP
#define LIBBITCOIN_BLOCKCHAIN_HEADER_POOL_HPP

#include <cstddef>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>
#include <bitcoin/blockchain/pools/header_branch.hpp>
#include <bitcoin/blockchain/pools/header_entry.hpp>

namespace libbitcoin {
namespace blockchain {

/// This class is thread safe against concurrent filtering only.
/// There is no search within headers of the header pool (just hashes).
class BCB_API header_pool
{
public:
    header_pool(size_t maximum_depth);

    /// The number of headers in the pool.
    size_t size() const;

    /// The header exists in the pool.
    bool exists(header_const_ptr header) const;

    /// Add newly-validated header.
    void add(header_const_ptr valid_header, size_t height);

    /// Add root path of reorganized headers (no branches).
    void add(header_const_ptr_list_const_ptr valid_headers, size_t height);

    /// Remove path of accepted headers (sub-branches moved to root).
    void remove(header_const_ptr_list_const_ptr accepted_headers);

    /// Purge branch rooted below top minus maximum depth.
    void prune(size_t top_height);

    /// Remove all message vectors that match header hashes.
    void filter(get_data_ptr message) const;

    /// Get the root path to and including the new header.
    /// This will be empty if the header already exists in the pool.
    header_branch::ptr get_branch(header_const_ptr candidate_header) const;

protected:
    // A bidirectional map is used for efficient header and position retrieval.
    // This produces the effect of a circular buffer hash table header forest.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<header_entry>,
        boost::bimaps::multiset_of<size_t>> header_entries;

    bool exists(const hash_digest& hash) const;
    void prune(const hash_list& hashes, size_t minimum_height);
    header_const_ptr parent(header_const_ptr header) const;
    ////void log_content() const;

    // This is thread safe.
    const size_t maximum_depth_;

    // This is guarded against filtering concurrent to writing.
    // All other operations are presumed to be externally protected.
    header_entries headers_;
    mutable upgrade_mutex mutex_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
