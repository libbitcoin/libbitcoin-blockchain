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
#ifndef LIBBITCOIN_BLOCKCHAIN_VALIDATE_HPP
#define LIBBITCOIN_BLOCKCHAIN_VALIDATE_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain/define.hpp>

namespace libbitcoin {
namespace blockchain {

// TODO: this is not an interface, collapse with validate_block_impl.
/// This class is not thread safe.
class BCB_API validate_block
{
public:
    code check_block();
    code accept_block() const;
    code connect_block() const;

    /// Required to call before calling accept_block or connect_block.
    void initialize_context();

protected:
    typedef std::vector<uint8_t> versions;
    typedef std::function<bool()> stopped_callback;

    validate_block(size_t height, const chain::block& block,
        bool testnet, const config::checkpoint::list& checks,
        stopped_callback stop_callback);

    virtual uint64_t median_time_past() const = 0;
    virtual uint32_t previous_block_bits() const = 0;
    virtual uint64_t actual_time_span(size_t interval) const = 0;
    virtual versions preceding_block_versions(size_t maximum) const = 0;
    virtual chain::header fetch_block(size_t fetch_height) const = 0;
    virtual bool fetch_transaction(chain::transaction& tx, size_t& tx_height,
        const hash_digest& tx_hash) const = 0;
    virtual bool is_output_spent(const chain::output_point& outpoint) const = 0;
    virtual bool is_output_spent(const chain::output_point& previous_output,
        size_t index_in_block, size_t input_index) const = 0;

    bool stopped() const;
    bool is_valid_version() const;
    bool is_active(chain::script_context flag) const;
    bool is_unspent(const chain::transaction& tx) const;
    bool contains_unspent_duplicates() const;
    uint32_t work_required(bool is_testnet) const;

    code check_transaction(const chain::transaction& tx,
        size_t index_in_block, uint64_t& fees, size_t& sigops) const;
    code check_inputs(const chain::transaction& tx, size_t index_in_block,
        uint64_t& value, size_t& sigops) const;
    code check_input(const chain::transaction& tx, size_t index_in_block,
        size_t input_index, uint64_t& value, size_t& sigops) const;
    code check_sigops(const  chain::script& output,
        const  chain::script& input, size_t& sigops) const;

private:
    bool testnet_;
    const size_t height_;
    size_t legacy_sigops_;
    uint32_t activations_;
    uint32_t minimum_version_;
    const chain::block& current_block_;
    const config::checkpoint::list& checkpoints_;
    const stopped_callback stop_callback_;
};

} // namespace blockchain
} // namespace libbitcoin

#endif
