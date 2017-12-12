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
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::blockchain;
using namespace bc::machine;

BOOST_AUTO_TEST_SUITE(validate_block_tests)

#ifdef WITH_CONSENSUS
    static const auto libconsensus = true;
#else
    static const auto libconsensus = false;
#endif

BOOST_AUTO_TEST_CASE(validate_block__native__block_438513_tx__valid)
{
    //// DEBUG [blockchain] Input validation failed (stack false)
    //// libconsensus : false
    //// forks        : 62
    //// outpoint     : 8e51d775e0896e03149d585c0655b3001da0c55068b0885139ac6ec34cf76ba0:0
    //// script       : a914faa558780a5767f9e3be14992a578fc1cbcf483087
    //// inpoint      : 6b7f50afb8448c39f4714a73d2b181d3e3233e84670bdfda8f141db668226c54:0
    //// transaction  : 0100000001a06bf74cc36eac395188b06850c5a01d00b355065c589d14036e89e075d7518e000000009d483045022100ba555ac17a084e2a1b621c2171fa563bc4fb75cd5c0968153f44ba7203cb876f022036626f4579de16e3ad160df01f649ffb8dbf47b504ee56dc3ad7260af24ca0db0101004c50632102768e47607c52e581595711e27faffa7cb646b4f481fe269bd49691b2fbc12106ad6704355e2658b1756821028a5af8284a12848d69a25a0ac5cea20be905848eb645fd03d3b065df88a9117cacfeffffff0158920100000000001976a9149d86f66406d316d44d58cbf90d71179dd8162dd388ac355e2658

    static const auto index = 0u;
    static const auto forks = 62u;
    static const auto encoded_script = "a914faa558780a5767f9e3be14992a578fc1cbcf483087";
    static const auto encoded_tx = "0100000001a06bf74cc36eac395188b06850c5a01d00b355065c589d14036e89e075d7518e000000009d483045022100ba555ac17a084e2a1b621c2171fa563bc4fb75cd5c0968153f44ba7203cb876f022036626f4579de16e3ad160df01f649ffb8dbf47b504ee56dc3ad7260af24ca0db0101004c50632102768e47607c52e581595711e27faffa7cb646b4f481fe269bd49691b2fbc12106ad6704355e2658b1756821028a5af8284a12848d69a25a0ac5cea20be905848eb645fd03d3b065df88a9117cacfeffffff0158920100000000001976a9149d86f66406d316d44d58cbf90d71179dd8162dd388ac355e2658";

    data_chunk decoded_tx;
    BOOST_REQUIRE(decode_base16(decoded_tx, encoded_tx));

    data_chunk decoded_script;
    BOOST_REQUIRE(decode_base16(decoded_script, encoded_script));

    transaction tx;
    BOOST_REQUIRE(tx.from_data(decoded_tx));

    const auto& input = tx.inputs()[index];
    auto& prevout = input.previous_output().validation.cache;

    prevout.set_script(script::factory_from_data(decoded_script, false));
    BOOST_REQUIRE(prevout.script().is_valid());

    const auto result = validate_input::verify_script(tx, index, 62, libconsensus);
    BOOST_REQUIRE_EQUAL(result.value(), error::success);
}

BOOST_AUTO_TEST_SUITE_END()
