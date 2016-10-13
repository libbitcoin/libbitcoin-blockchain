###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-blockchain developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Standard declarations.
#==============================================================================
# Requires Automake 1.14 or newer.

# Declare the required version of Autoconf.
AC_PREREQ([2.65])

# Process command-line arguments and perform initialization and verification.
AC_INIT([libbitcoin-blockchain], [3.0.0], [eric@voskuil.org])

# Do compilation tests.
AC_LANG(C++)

# Specify the temporary directory for build tools.
AC_CONFIG_AUX_DIR([build-aux])

# Specify the directory of additional local Autoconf macros.
AC_CONFIG_MACRO_DIR([m4])

# Run macros for operation of generated Makefiles, enable non-recursive make.
# Unless [foreign] is specified standard GNU files will be required,
# specifically: AUTHORS, COPYING, INSTALL, NEWS, README and ChangeLog.
AM_INIT_AUTOMAKE([subdir-objects])

# Enable C and POSIX extensions that may be disabled on certain platforms.
AC_USE_SYSTEM_EXTENSIONS

# Enable the archiver.
AM_PROG_AR

# Initialize libtool.
LT_INIT

# Determine C++ compiler to use.
AC_PROG_CXX

# Enable shared libraries if available, and static if they don't conflict.
AC_PROG_LIBTOOL

# Compute the canonical host-system type variable host, including host_os.
AC_CANONICAL_HOST

# Enable silent rules option.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for pkg-config.
PKG_PROG_PKG_CONFIG

# Declare environment variables that affect the build.
#------------------------------------------------------------------------------
AC_ARG_VAR([CC], "C compiler to use, such as gcc or clang")
AC_ARG_VAR([CXX], "C++ compiler to use, such as g++ or clang++")
AC_ARG_VAR([PKG_CONFIG_PATH], "Additional directories for package discovery.")

# Check for baseline language coverage in the compiler for the C++11 standard.
#------------------------------------------------------------------------------
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])


# Process options.
#==============================================================================
# Implement --with-consensus and define WITH_CONSENSUS.
#------------------------------------------------------------------------------
AC_MSG_CHECKING([--with-consensus option])
AC_ARG_WITH([consensus],
    AS_HELP_STRING([--with-consensus],
        [Link libbitcoin-consensus and use for consensus checks. @<:@default=yes@:>@]),
    [with_consensus=$withval],
    [with_consensus=yes])
AC_MSG_RESULT([$with_consensus])
AS_CASE([${with_consensus}], [yes], AC_DEFINE([WITH_CONSENSUS]))

# Implement --with-pkgconfigdir and output ${pkgconfigdir}.
#------------------------------------------------------------------------------
AC_MSG_CHECKING([--with-pkgconfigdir option])
AC_ARG_WITH([pkgconfigdir],
    AS_HELP_STRING([--with-pkgconfigdir=DIR],
        [Path to pkgconfig directory. @<:@default=${libdir}/pkgconfig@:>@]),
    [pkgconfigdir=$withval],
    [pkgconfigdir=${libdir}/pkgconfig])
AC_MSG_RESULT([$pkgconfigdir])
AC_SUBST([pkgconfigdir])

# Implement --with-tests and declare WITH_TESTS.
#------------------------------------------------------------------------------
AC_MSG_CHECKING([--with-tests option])
AC_ARG_WITH([tests],
    AS_HELP_STRING([--with-tests],
        [Compile with unit tests. @<:@default=yes@:>@]),
    [with_tests=$withval],
    [with_tests=yes])
AC_MSG_RESULT([$with_tests])
AM_CONDITIONAL([WITH_TESTS], [test x$with_tests != xno])

# Implement --with-tools and declare WITH_TOOLS.
#------------------------------------------------------------------------------
AC_MSG_CHECKING([--with-tools option])
AC_ARG_WITH([tools],
    AS_HELP_STRING([--with-tools],
        [Compile with tools. @<:@default=yes@:>@]),
    [with_tools=$withval],
    [with_tools=yes])
AC_MSG_RESULT([$with_tools])
AM_CONDITIONAL([WITH_TOOLS], [test x$with_tools != xno])

# Implement --enable-ndebug and define NDEBUG.
#------------------------------------------------------------------------------
AC_MSG_CHECKING([--enable-ndebug option])
AC_ARG_ENABLE([ndebug],
    AS_HELP_STRING([--enable-ndebug],
        [Compile without debug assertions. @<:@default=yes@:>@]),
    [enable_ndebug=$enableval],
    [enable_ndebug=yes])
AC_MSG_RESULT([$enable_ndebug])
AS_CASE([${enable_ndebug}], [yes], AC_DEFINE([NDEBUG]))

# Inherit --enable-shared and define BOOST_ALL_DYN_LINK.
#------------------------------------------------------------------------------
AS_CASE([${enable_shared}], [yes], AC_DEFINE([BOOST_ALL_DYN_LINK]))


# Check dependencies.
#==============================================================================
# Require Boost of at least version 1.56.0 and output ${boost_CPPFLAGS/LDFLAGS}.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_BOOST_BASE([1.56.0],
        [AC_SUBST([boost_CPPFLAGS], [${BOOST_CPPFLAGS}])
         AC_SUBST([boost_LDFLAGS], [${BOOST_LDFLAGS}])
         AC_MSG_NOTICE([boost_CPPFLAGS : ${boost_CPPFLAGS}])
         AC_MSG_NOTICE([boost_LDFLAGS : ${boost_LDFLAGS}])],
        [AC_MSG_ERROR([Boost 1.56.0 or later is required but was not found.])])])

AS_CASE([${with_tests}], [yes],
    [AX_BOOST_UNIT_TEST_FRAMEWORK
     AC_SUBST([boost_unit_test_framework_LIBS], [${BOOST_UNIT_TEST_FRAMEWORK_LIB}])
     AC_MSG_NOTICE([boost_unit_test_framework_LIBS : ${boost_unit_test_framework_LIBS}])],
    [AC_SUBST([boost_unit_test_framework_LIBS], [])])

# Require bitcoin-database of at least version 3.0.0 and output ${bitcoin_database_CPPFLAGS/LIBS/PKG}.
#------------------------------------------------------------------------------
PKG_CHECK_MODULES([bitcoin_database], [libbitcoin-database >= 3.0.0])
AC_SUBST([bitcoin_database_PKG], ['libbitcoin-database >= 3.0.0'])
AC_SUBST([bitcoin_database_CPPFLAGS], [${bitcoin_database_CFLAGS}])
AC_MSG_NOTICE([bitcoin_database_CPPFLAGS : ${bitcoin_database_CPPFLAGS}])
AC_MSG_NOTICE([bitcoin_database_LIBS : ${bitcoin_database_LIBS}])

# Require bitcoin-consensus of at least version 2.0.0 and output ${bitcoin_consensus_CPPFLAGS/LIBS/PKG}.
#------------------------------------------------------------------------------
AS_CASE([${with_consensus}], [yes],
    [PKG_CHECK_MODULES([bitcoin_consensus], [libbitcoin-consensus >= 2.0.0])
     AC_SUBST([bitcoin_consensus_PKG], ['libbitcoin-consensus >= 2.0.0'])
     AC_SUBST([bitcoin_consensus_CPPFLAGS], [${bitcoin_consensus_CFLAGS}])
     AC_MSG_NOTICE([bitcoin_consensus_CPPFLAGS : ${bitcoin_consensus_CPPFLAGS}])
     AC_MSG_NOTICE([bitcoin_consensus_LIBS : ${bitcoin_consensus_LIBS}])],
    [AC_SUBST([bitcoin_consensus_PKG], [])])


# Set flags.
#==============================================================================
# Require c++11 for all c++ products.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_COMPILE_FLAG([-std=c++11],
        [CXXFLAGS="$CXXFLAGS -std=c++11"])])

# Warn on all stuff.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_COMPILE_FLAG([-Wall],
        [CFLAGS="$CFLAGS -Wall"])])

# Warn on extra stuff.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_COMPILE_FLAG([-Wextra],
        [CFLAGS="$CFLAGS -Wextra"])])

# Be really annoying.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_COMPILE_FLAG([-Wpedantic],
        [CFLAGS="$CFLAGS -Wpedantic"],
        [AX_CHECK_COMPILE_FLAG([-pedantic],
            [CFLAGS="$CFLAGS -pedantic"])])])

# Conform to style.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_COMPILE_FLAG([-Wno-missing-braces],
        [CXXFLAGS="$CXXFLAGS -Wno-missing-braces"])])

# Conflict in stdlib under clang. Enabled in clang only.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*clang*],
    [AX_CHECK_COMPILE_FLAG([-Wno-mismatched-tags],
        [CXXFLAGS="$CXXFLAGS -Wno-mismatched-tags"])])

# Clean up boost 1.55 headers. Enabled in gcc only.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*gcc*],
    [AX_CHECK_COMPILE_FLAG([-Wno-deprecated-declarations],
        [CXXFLAGS="$CXXFLAGS -Wno-deprecated-declarations"])])

# Protect stack.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_LINK_FLAG([-fstack-protector],
        [LDFLAGS="$LDFLAGS -fstack-protector"])])

# Protect stack comprehensively.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*],
    [AX_CHECK_LINK_FLAG([-fstack-protector-all],
        [LDFLAGS="$LDFLAGS -fstack-protector-all"])])

# Hide internal functions from external libs. Enabled in gcc only.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*gcc*],
    [AX_CHECK_COMPILE_FLAG([-fvisibility-hidden],
        [CXXFLAGS="$CXXFLAGS -fvisibility-hidden"])])

# Hide inlines from external libs. Enabled in gcc only.
#------------------------------------------------------------------------------
AS_CASE([${CC}], [*gcc*],
    [AX_CHECK_COMPILE_FLAG([-fvisibility-inlines-hidden],
        [CXXFLAGS="$CXXFLAGS -fvisibility-inlines-hidden"])])


# Process outputs into templates.
#==============================================================================
AC_CONFIG_FILES([Makefile libbitcoin-blockchain.pc])
AC_OUTPUT
