/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

// Sponsored in part by Digital Contract Design, LLC

#include <bitcoin/blockchain/block_chain_initializer.hpp>

namespace libbitcoin {
namespace blockchain {

using namespace bc::system;
using namespace bc::system::chain;

block_chain_initializer::block_chain_initializer(
    const blockchain::settings& settings,
    const database::settings& database_settings,
    const system::settings& system_settings)
  : database_(database_settings, settings.index_payments, settings.bip158),
    settings_chain_(settings), settings_database_(database_settings),
    settings_system_(system_settings)
{
}

block_chain_initializer::~block_chain_initializer()
{
}

code block_chain_initializer::create(
    const chain::block& genesis)
{
    code ec = error::success;

    if ((ec = populate_neutrino_filter_metadata(genesis, 0)))
        return ec;

    if (!database_.create(genesis))
       ec = error::operation_failed;

    return ec;
}

code block_chain_initializer::push(const chain::block& block,
    size_t height, uint32_t median_time_past)
{
    code ec = error::success;

    if ((ec = populate_neutrino_filter_metadata(block, height)))
        return ec;

    if (!database_.push(block, height, median_time_past))
       ec = error::operation_failed;

    return ec;
}

code block_chain_initializer::populate_neutrino_filter_metadata(
    const chain::block& block, size_t height)
{
    static const auto form = "Block [%s] Filter Header [%s] Filter [%s]";

    if (!settings_chain_.bip158)
        return error::success;

    auto previous_filter_header = null_hash;

    if (height != 0)
    {
        const auto result_block_previous = database_.blocks().get(
            block.header().previous_block_hash());

        if (!result_block_previous)
            return error::operation_failed;

        const auto result_filter = database_.neutrino_filters().get(
            result_block_previous.neutrino_filter());

        if (!result_filter)
            return error::operation_failed;

        previous_filter_header = result_filter.header();
    }

    data_chunk filter;
    if (!neutrino::compute_filter(block, filter))
    {
        LOG_ERROR(LOG_BLOCKCHAIN)
            << boost::format(form) %
                encode_hash(block.hash());

        return error::metadata_prevout_missing;
    }

    const auto filter_header = neutrino::compute_filter_header(
        previous_filter_header, filter);

    block.header().metadata.neutrino_filter = std::make_shared<block_filter>(
        neutrino_filter_type, block.hash(), filter_header, filter);

    LOG_VERBOSE(LOG_BLOCKCHAIN)
        << boost::format(form) %
            encode_hash(block.hash()) %
            encode_hash(filter_header) %
            encode_base16(filter);

    return error::success;
}

} // namespace blockchain
} // namespace libbitcoin
