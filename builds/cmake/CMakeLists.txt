###############################################################################
#  Copyright (c) 2014-2019 libbitcoin-blockchain developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# libbitcoin-blockchain project configuration.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(libbitcoin-blockchain LANGUAGES C CXX)

enable_testing()

list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules" )
include(CheckIncludeFiles)
include(CheckSymbolExists)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
  set( CANONICAL_LIB_NAME libbitcoin-blockchain )
else ()
  set( CANONICAL_LIB_NAME bitcoin-blockchain )
  find_package( PkgConfig REQUIRED )

  set( prefix "${CMAKE_PREFIX_PATH}" )
  set( exec_prefix "\${prefix}" )
  set( libdir "\${exec_prefix}/lib" )
  set( includedir "\${exec_prefix}/include" )

  set( PACKAGE_VERSION "4.0.0" )
  set( VERSION "${PACKAGE_VERSION}" )
endif ()

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Add compiler options
#------------------------------------------------------------------------------
# Warn on all stuff.
add_compile_options( "-Wall" )

# Warn on extra stuff.
add_compile_options( "-Wextra" )

# Be really annoying.
add_compile_options( "-Wpedantic" )

# Disallow warning on style order of declarations.
add_compile_options( "-Wno-reorder" )

# Suppress warning for incomplete field initialization.
add_compile_options( "-Wno-missing-field-initializers" )

# Conform to style.
add_compile_options( "-Wno-missing-braces" )

# Ignore comments within comments or commenting of backslash extended lines.
add_compile_options( "-Wno-comment" )

# Conflict in stdlib under clang.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options( "-Wno-mismatched-tags" )
endif()

# Implement -Dwith-consensus and define WITH_CONSENSUS.
#------------------------------------------------------------------------------
set( with-consensus "yes" CACHE BOOL "Link libbitcoin-consensus and use for consensus checks." )

if (with-consensus)
    add_definitions( -DWITH_CONSENSUS )
endif()

# Implement -Dpkgconfigdir and output ${pkgconfigdir}.
#------------------------------------------------------------------------------
set( pkgconfigdir "${libdir}/pkgconfig" CACHE PATH "Path to pkgconfig directory." )

# Implement -Dwith-tests and declare with-tests.
#------------------------------------------------------------------------------
set( with-tests "yes" CACHE BOOL "Compile with unit tests." )

# Implement -Dwith-tools and declare with-tools.
#------------------------------------------------------------------------------
set( with-tools "yes" CACHE BOOL "Compile with tools." )

# Implement -Denable-ndebug and define NDEBUG.
#------------------------------------------------------------------------------
set( enable-ndebug "yes" CACHE BOOL "Compile without debug assertions." )

if (enable-ndebug)
    add_definitions( -DNDEBUG )
endif()

# Inherit -Denable-shared and define BOOST_ALL_DYN_LINK.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    add_definitions( -DBOOST_ALL_DYN_LINK )
endif()

# Find boost
#------------------------------------------------------------------------------
find_package( Boost 1.62.0 REQUIRED COMPONENTS
    unit_test_framework )

set( boost_unit_test_framework_LIBS "-lboost_unit_test_framework" )

if (enable-ndebug)
    set( Boost_LIBRARY_DIR "${Boost_LIBRARY_DIR_DEBUG}" )
else ()
    set( Boost_LIBRARY_DIR "${Boost_LIBRARY_DIR_RELEASE}" )
endif()

set( boost_CPPFLAGS "-I${Boost_INCLUDE_DIR}" )
set( boost_LDFLAGS "-L${Boost_LIBRARY_DIR}" )

# Find bitcoin-database
#------------------------------------------------------------------------------
find_package( Bitcoin-Database 4.0.0 REQUIRED )

# Find bitcoin-consensus
#------------------------------------------------------------------------------
if (with-consensus)
    find_package( Bitcoin-Consensus 4.0.0 REQUIRED )
endif()

# Define project common includes directories
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    include_directories( SYSTEM
        ${bitcoin_database_INCLUDE_DIRS}
        ${bitcoin_consensus_INCLUDE_DIRS} )
else()
    include_directories( SYSTEM
        ${bitcoin_database_STATIC_INCLUDE_DIRS}
        ${bitcoin_consensus_STATIC_INCLUDE_DIRS} )
endif()

# Define project common library directories
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    link_directories( 
        ${bitcoin_database_LIBRARY_DIRS}
        ${bitcoin_consensus_LIBRARY_DIRS} )
else()
    link_directories( 
        ${bitcoin_database_STATIC_LIBRARY_DIRS}
        ${bitcoin_consensus_STATIC_LIBRARY_DIRS} )
endif()

# Define project common libraries/linker flags.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    link_libraries( 
        "-fstack-protector"
        "-fstack-protector-all"
        ${bitcoin_database_LIBRARIES}
        ${bitcoin_consensus_LIBRARIES} )
else()
    link_libraries( 
        "-fstack-protector"
        "-fstack-protector-all"
        ${bitcoin_database_STATIC_LIBRARIES}
        ${bitcoin_consensus_STATIC_LIBRARIES} )
endif()

# Define ${CANONICAL_LIB_NAME} project.
#------------------------------------------------------------------------------
add_library( ${CANONICAL_LIB_NAME}
    "../../src/settings.cpp"
    "../../src/interface/block_chain.cpp"
    "../../src/organizers/organize_block.cpp"
    "../../src/organizers/organize_header.cpp"
    "../../src/organizers/organize_transaction.cpp"
    "../../src/pools/block_entry.cpp"
    "../../src/pools/block_pool.cpp"
    "../../src/pools/header_branch.cpp"
    "../../src/pools/header_entry.cpp"
    "../../src/pools/header_pool.cpp"
    "../../src/pools/transaction_entry.cpp"
    "../../src/pools/transaction_pool.cpp"
    "../../src/pools/utilities/anchor_converter.cpp"
    "../../src/pools/utilities/child_closure_calculator.cpp"
    "../../src/pools/utilities/conflicting_spend_remover.cpp"
    "../../src/pools/utilities/parent_closure_calculator.cpp"
    "../../src/pools/utilities/priority_calculator.cpp"
    "../../src/pools/utilities/stack_evaluator.cpp"
    "../../src/pools/utilities/transaction_order_calculator.cpp"
    "../../src/pools/utilities/transaction_pool_state.cpp"
    "../../src/populate/populate_base.cpp"
    "../../src/populate/populate_block.cpp"
    "../../src/populate/populate_chain_state.cpp"
    "../../src/populate/populate_header.cpp"
    "../../src/populate/populate_transaction.cpp"
    "../../src/validate/validate_block.cpp"
    "../../src/validate/validate_header.cpp"
    "../../src/validate/validate_input.cpp"
    "../../src/validate/validate_transaction.cpp" )

# ${CANONICAL_LIB_NAME} project specific include directories.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
        "../../include"
        ${bitcoin_database_INCLUDE_DIRS}
        ${bitcoin_consensus_INCLUDE_DIRS} )
else()
    target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
        "../../include"
        ${bitcoin_database_STATIC_INCLUDE_DIRS}
        ${bitcoin_consensus_STATIC_INCLUDE_DIRS} )
endif()

target_include_directories( ${CANONICAL_LIB_NAME} PUBLIC
"../../include" )

# ${CANONICAL_LIB_NAME} project specific libraries/linker flags.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    target_link_libraries( ${CANONICAL_LIB_NAME}
        ${bitcoin_database_LIBRARIES}
        ${bitcoin_consensus_LIBRARIES} )
else()
    target_link_libraries( ${CANONICAL_LIB_NAME}
        ${bitcoin_database_STATIC_LIBRARIES}
        ${bitcoin_consensus_STATIC_LIBRARIES} )
endif()

# Define libbitcoin-blockchain-test project.
#------------------------------------------------------------------------------
if (with-tests)
    add_executable( libbitcoin-blockchain-test
        "../../test/main.cpp"
        "../../test/utility.cpp"
        "../../test/utility.hpp"
        "../../test/interface/fast_chain.cpp"
        "../../test/interface/safe_chain.cpp"
        "../../test/pools/block_entry.cpp"
        "../../test/pools/block_pool.cpp"
        "../../test/pools/header_branch.cpp"
        "../../test/pools/header_entry.cpp"
        "../../test/pools/header_pool.cpp"
        "../../test/pools/transaction_entry.cpp"
        "../../test/pools/transaction_pool.cpp"
        "../../test/pools/utilities/anchor_converter.cpp"
        "../../test/pools/utilities/child_closure_calculator.cpp"
        "../../test/pools/utilities/conflicting_spend_remover.cpp"
        "../../test/pools/utilities/parent_closure_calculator.cpp"
        "../../test/pools/utilities/priority_calculator.cpp"
        "../../test/pools/utilities/transaction_order_calculator.cpp"
        "../../test/pools/utilities/utilities.cpp"
        "../../test/pools/utilities/utilities.hpp"
        "../../test/validators/validate_block.cpp"
        "../../test/validators/validate_transaction.cpp" )

    add_test( NAME libbitcoin-blockchain-test COMMAND libbitcoin-blockchain-test
            --run_test=*
            --show_progress=no
            --detect_memory_leak=0
            --report_level=no
            --build_info=yes )

#     libbitcoin-blockchain-test project specific include directories.
#------------------------------------------------------------------------------
    target_include_directories( libbitcoin-blockchain-test PRIVATE
        "../../include" )

#     libbitcoin-blockchain-test project specific libraries/linker flags.
#------------------------------------------------------------------------------
    target_link_libraries( libbitcoin-blockchain-test
        ${CANONICAL_LIB_NAME}
        ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )

endif()

# Define initchain project.
#------------------------------------------------------------------------------
if (with-tools)
    add_executable( initchain
        "../../tools/initchain/initchain.cpp" )

#     initchain project specific include directories.
#------------------------------------------------------------------------------
    target_include_directories( initchain PRIVATE
        "../../include" )

#     initchain project specific libraries/linker flags.
#------------------------------------------------------------------------------
    target_link_libraries( initchain
        ${CANONICAL_LIB_NAME} )

endif()

# Manage pkgconfig installation.
#------------------------------------------------------------------------------
configure_file(
  "../../libbitcoin-blockchain.pc.in"
  "libbitcoin-blockchain.pc" @ONLY )

install( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-blockchain.pc"
    DESTINATION "${pkgconfigdir}" )

# Manage installation of docs.
#------------------------------------------------------------------------------
install( FILES
    "../../AUTHORS"
    "../../COPYING"
    "../../ChangeLog"
    "../../INSTALL"
    "../../NEWS"
    "../../README"
    DESTINATION share/doc/libbitcoin-blockchain )

# Manage ${CANONICAL_LIB_NAME} installation.
#------------------------------------------------------------------------------
install( TARGETS ${CANONICAL_LIB_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include )

# Manage include installation.
#------------------------------------------------------------------------------
install( DIRECTORY "../../include/bitcoin"
    DESTINATION include )

