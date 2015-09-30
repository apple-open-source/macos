/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// utility_config.h - common configuration for the utility libraries
//
#ifndef _H_UTILITY_CONFIG
#define _H_UTILITY_CONFIG

#include <CoreFoundation/CFBase.h>
#include <assert.h>

//
// Decide what io apis we'll be using
//
#define _USE_IO_POSIX 0
#define _USE_IO_MACOS 1

#if !defined(_USE_IO)
# if TARGET_API_MAC_OS8
#  define _USE_IO _USE_IO_MACOS
# else
#  define _USE_IO _USE_IO_POSIX
# endif
#endif

//
// Decide what threading support we'll be using
//
#define _USE_NO_THREADS 0
#define _USE_PTHREADS 1
#define _USE_MPTHREADS 2

#include <unistd.h>
#if defined(_POSIX_THREADS)
# define _USE_THREADS _USE_PTHREADS
#endif
#if !defined(_USE_THREADS)
# define _USE_THREADS _USE_NO_THREADS
#endif


//
// Compatibility switches
//
#define COMPAT_OSX_10_0		1	/* be compatible with MacOS 10.0.x formats & features */


//
// Bugs, buglets, and special compiler features
//
#define bug_private	private
#define bug_protected protected
#define bug_const const

#define BUG_GCC 0

#if defined(__GNUC__)
# undef BUG_GCC
# define BUG_GCC 1
# undef bug_const
# define bug_const
#else
# if !defined(__attribute__)
#  define __attribute__(whatever)	/* don't use for non-gcc compilers */
# endif
#endif

/*
ld: for architecture ppc
ld: common symbols not allowed with MH_DYLIB output format
/Network/Servers/fivestar/homes/delaware/jhurley/AppleDev/insight/build/intermediates/KeychainLib.build/Objects/Sources/KeychainLib/KCSleep.o definition of common __7KCSleep.mKCSleepRec (size 12)
*/
#define BUG_COMMON_SYMBOLS

// Make sure that namespace Security exists
namespace Security
{
} // end namespace Security

// Automatically use the Security namespace for everything that includes the utility_config header.
using namespace Security;

// Make sure that namespace std exists
namespace std
{
} // end namespace std

// Automatically use the std namespace for everything that includes the utility_config header.
using namespace std;

#endif //_H_UTILITY_CONFIG
