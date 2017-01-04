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
#include <bitcoin/blockchain/validate/validate_input.hpp>

#include <cstdint>
#include <bitcoin/bitcoin.hpp>

#ifdef WITH_CONSENSUS
#include <bitcoin/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

using namespace bc::chain;
using namespace bc::machine;

#ifdef WITH_CONSENSUS

using namespace bc::consensus;

uint32_t validate_input::convert_flags(uint32_t native_flags)
{
    uint32_t flags = verify_flags_none;

    if (script::is_enabled(native_flags, rule_fork::bip16_rule))
        flags |= verify_flags_p2sh;

    if (script::is_enabled(native_flags, rule_fork::bip65_rule))
        flags |= verify_flags_checklocktimeverify;

    if (script::is_enabled(native_flags, rule_fork::bip66_rule))
        flags |= verify_flags_dersig;

    return flags;
}

code validate_input::convert_result(verify_result_type result)
{
    switch (result)
    {
        // Logical true result.
        case verify_result_type::verify_result_eval_true:
            return error::success;

        // Logical false result.
        case verify_result_type::verify_result_eval_false:
            return error::stack_false;

        // Max size errors.
        case verify_result_type::verify_result_script_size:
        case verify_result_type::verify_result_push_size:
        case verify_result_type::verify_result_op_count:
        case verify_result_type::verify_result_stack_size:
        case verify_result_type::verify_result_sig_count:
        case verify_result_type::verify_result_pubkey_count:
            return error::invalid_script;

        // Failed verify operations.
        case verify_result_type::verify_result_verify:
        case verify_result_type::verify_result_equalverify:
        case verify_result_type::verify_result_checkmultisigverify:
        case verify_result_type::verify_result_checksigverify:
        case verify_result_type::verify_result_numequalverify:
            return error::invalid_script;

        // Logical/Format/Canonical errors.
        case verify_result_type::verify_result_bad_opcode:
        case verify_result_type::verify_result_disabled_opcode:
        case verify_result_type::verify_result_invalid_stack_operation:
        case verify_result_type::verify_result_invalid_altstack_operation:
        case verify_result_type::verify_result_unbalanced_conditional:
            return error::invalid_script;

        // Softbranch safeness (should not see).
        case verify_result_type::verify_result_discourage_upgradable_nops:
            return error::operation_failed;

        // BIP62 errors (should not see).
        case verify_result_type::verify_result_sig_hashtype:
        case verify_result_type::verify_result_sig_der:
        case verify_result_type::verify_result_minimaldata:
        case verify_result_type::verify_result_sig_pushonly:
        case verify_result_type::verify_result_sig_high_s:
        case verify_result_type::verify_result_sig_nulldummy:
        case verify_result_type::verify_result_pubkeytype:
        case verify_result_type::verify_result_cleanstack:
            return error::operation_failed;

        // BIP65 errors.
        case verify_result_type::verify_result_negative_locktime:
        case verify_result_type::verify_result_unsatisfied_locktime:
            return error::invalid_script;

        // Other errors.
        case verify_result_type::verify_result_op_return:
        case verify_result_type::verify_result_unknown_error:
            return error::invalid_script;

        // Augmention codes for tx deserialization.
        case verify_result_type::verify_result_tx_invalid:
        case verify_result_type::verify_result_tx_size_invalid:
        case verify_result_type::verify_result_tx_input_invalid:
            return error::invalid_script;

        default:
            return error::invalid_script;
    }
}

code validate_input::verify_script(const transaction& tx, uint32_t input_index,
    uint32_t branches, bool use_libconsensus)
{
    if (!use_libconsensus)
    {
        ////// Simulate the inefficiency of calling libconsensus.
        ////BITCOIN_ASSERT(input_index < tx.inputs().size());
        ////const auto& prevout = tx.inputs()[input_index].previous_output().validation;
        ////const auto script_data = prevout.cache.script().to_data(false);
        ////const auto tx_data = tx.to_data();
        ////auto clone = transaction::factory_from_data(tx_data);
        ////const auto input = clone.inputs()[input_index].script();
        ////const auto prevout = script::factory_from_data(script_data, false);
        ////return script::verify(clone, input_index, branches, input, prevout);
        return script::verify(tx, input_index, branches);
    }

    BITCOIN_ASSERT(input_index < tx.inputs().size());
    const auto& prevout = tx.inputs()[input_index].previous_output().validation;
    const auto script_data = prevout.cache.script().to_data(false);
    const auto tx_data = tx.to_data();

    // libconsensus
    return convert_result(consensus::verify_script(tx_data.data(),
        tx_data.size(), script_data.data(), script_data.size(), input_index,
        convert_flags(branches)));
}

#else

code validate_input::verify_script(const transaction& tx,
    uint32_t input_index, uint32_t branches, bool use_libconsensus)
{
    if (use_libconsensus)
        return error::operation_failed;

    return script::verify(tx, input_index, branches);
}

#endif

} // namespace blockchain
} // namespace libbitcoin
