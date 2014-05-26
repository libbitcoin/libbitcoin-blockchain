/*
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BLOCKCHAIN_DEFINE_HPP
#define LIBBITCOIN_BLOCKCHAIN_DEFINE_HPP

// See http://gcc.gnu.org/wiki/Visibility

// Generic helper definitions for shared library support
#if defined _MSC_VER || defined __CYGWIN__
    #define BCB_HELPER_DLL_IMPORT __declspec(dllimport)
    #define BCB_HELPER_DLL_EXPORT __declspec(dllexport)
    #define BCB_HELPER_DLL_LOCAL
#else
    #if __GNUC__ >= 4
        #define BCB_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
        #define BCB_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
        #define BCB_HELPER_DLL_LOCAL  __attribute__ ((visibility ("internal")))
    #else
        #define BCB_HELPER_DLL_IMPORT
        #define BCB_HELPER_DLL_EXPORT
        #define BCB_HELPER_DLL_LOCAL
    #endif
#endif

// Now we use the generic helper definitions above to
// define BCB_API and BCB_INTERNAL.
// BCB_API is used for the public API symbols. It either DLL imports or
// DLL exports (or does nothing for static build)
// BCB_INTERNAL is used for non-api symbols.

#if defined BCB_STATIC
    #define BCB_API
    #define BCB_INTERNAL
#elif defined BCB_DLL
    #define BCB_API      BCB_HELPER_DLL_EXPORT
    #define BCB_INTERNAL BCB_HELPER_DLL_LOCAL
#else
    #define BCB_API      BCB_HELPER_DLL_IMPORT
    #define BCB_INTERNAL BCB_HELPER_DLL_LOCAL
#endif

// Tag to deprecate functions and methods.
// Gives a compiler warning when they are used.
#if defined _MSC_VER || defined __CYGWIN__
    #define BCB_DEPRECATED __declspec(deprecated)
#else
    #if __GNUC__ >= 4
        #define BCB_DEPRECATED __attribute__((deprecated))
    #else
        #define BCB_DEPRECATED
    #endif
#endif

#endif

