AC_INIT([libbitcoin-blockchain], [0.4.0], [genjix@riseup.net])
AC_USE_SYSTEM_EXTENSIONS
AC_LANG(C++)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])
AM_PROG_AR
LT_INIT
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_GNU_SOURCE
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Boost doesn't publish package configs but has m4 tests.
# The current versions produce incorrect error message text.
AX_BOOST_BASE([1.50],,
    [AC_MSG_ERROR([Boost 1.50 or later is required but was not found.])])
AX_BOOST_UNIT_TEST_FRAMEWORK

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([bitcoin], [libbitcoin >= 2.4.0])

# Do not add contextual build parameters in configuration.
# http://stackoverflow.com/a/4680578
OPTIMIZATIONS="-fvisibility=internal"
WARNINGS="-Wall -Wextra -Wpedantic -Wno-missing-braces"
SAFEGUARDS="-fvisibility-inlines-hidden -fstack-protector-all"
EXCEPTIONS="-Wno-unused-parameter"
AM_CXXFLAGS=$WARNINGS" "$SAFEGUARDS" "$OPTIMIZATIONS" "$EXCEPTIONS
AC_SUBST([AM_CXXFLAGS])

AC_ARG_WITH([pkgconfigdir], AS_HELP_STRING([--with-pkgconfigdir=PATH],
    [Path to the pkgconfig directory [[LIBDIR/pkgconfig]]]),
    [pkgconfigdir="$withval"], [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST([pkgconfigdir])

AC_CONFIG_FILES([Makefile include/bitcoin/Makefile src/Makefile libbitcoin-blockchain.pc])
AC_OUTPUT

