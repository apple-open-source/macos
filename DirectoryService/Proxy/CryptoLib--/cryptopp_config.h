/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef CRYPTOPP_CONFIG_H
#define CRYPTOPP_CONFIG_H

// ***************** Important Settings ********************

// define this if running on a big-endian CPU
#if defined(__sparc__) || defined(__hppa__) || defined(__ppc__) || defined(__mips__) || (defined(__MWERKS__) && !defined(__INTEL__))
#define IS_BIG_ENDIAN
#endif

// define this if running on a little-endian CPU
// big endian will be assumed if IS_LITTLE_ENDIAN is not defined
#ifndef IS_BIG_ENDIAN
#define IS_LITTLE_ENDIAN
#endif

// define this if you want to disable all OS-dependent features,
// such as sockets and OS-provided random number generators
// #define NO_OS_DEPENDENCE

// Define this to use features provided by Microsoft's CryptoAPI.
// Currently the only feature used is random number generation.
// This macro will be ignored if NO_OS_DEPENDENCE is defined.
#define USE_MS_CRYPTOAPI

// define this if your compiler does not support namespaces
// #define NO_NAMESPACE
#ifdef NO_NAMESPACE
#define std
#define CryptoPP
#define USING_NAMESPACE(x)
#define NAMESPACE_BEGIN(x)
#define NAMESPACE_END
#define ANONYMOUS_NAMESPACE_BEGIN
#else
#define USING_NAMESPACE(x) using namespace x;
#define NAMESPACE_BEGIN(x) namespace x {
#define ANONYMOUS_NAMESPACE_BEGIN namespace {
#define NAMESPACE_END }
#endif

// ***************** Less Important Settings ***************

// switch between different secure memory allocation mechnisms, this is the only
// one available right now
#define SECALLOC_DEFAULT

#define GZIP_OS_CODE 0

// Try this if your CPU has 256K internal cache or a slow multiply instruction
// and you want a (possibly) faster IDEA implementation using log tables
// #define IDEA_LARGECACHE

// Try this if you have a large cache or your CPU is slow manipulating
// individual bytes.
// #define DIAMOND_USE_PERMTABLE

// Define this if, for the linear congruential RNG, you want to use
// the original constants as specified in S.K. Park and K.W. Miller's
// CACM paper.
// #define LCRNG_ORIGINAL_NUMBERS

// choose which style of sockets to wrap (mostly useful for cygwin which has both)
#define PREFER_BERKELEY_STYLE_SOCKETS
// #define PREFER_WINDOWS_STYLE_SOCKETS

// ***************** Important Settings Again ********************
// But the defaults should be ok.

typedef unsigned char byte;     // moved outside namespace for Borland C++Builder 5

NAMESPACE_BEGIN(CryptoPP)

typedef unsigned short word16;
#if defined(__alpha) && !defined(_MSC_VER)
typedef unsigned int word32;
#else
typedef unsigned long word32;
#endif

#if defined(__GNUC__) || defined(__MWERKS__)
#define WORD64_AVAILABLE
typedef unsigned long long word64;
#define W64LIT(x) x##LL
#elif defined(_MSC_VER) || defined(__BCPLUSPLUS__)
#define WORD64_AVAILABLE
typedef unsigned __int64 word64;
#define W64LIT(x) x##ui64
#endif

// defined this if your CPU is not 64-bit
#if defined(WORD64_AVAILABLE) && !defined(__alpha)
#define SLOW_WORD64
#endif

// word should have the same size as your CPU registers
// dword should be twice as big as word

#if (defined(__GNUC__) && !defined(__alpha)) || defined(__MWERKS__)
typedef unsigned long word;
typedef unsigned long long dword;
#elif defined(_MSC_VER) || defined(__BCPLUSPLUS__)
typedef unsigned __int32 word;
typedef unsigned __int64 dword;
#else
typedef unsigned int word;
typedef unsigned long dword;
#endif

const unsigned int WORD_SIZE = sizeof(word);
const unsigned int WORD_BITS = WORD_SIZE * 8;

#define LOW_WORD(x) (word)(x)

union dword_union
{
	dword_union (const dword &dw) : dw(dw) {}
	dword dw;
	word w[2];
};

#ifdef IS_LITTLE_ENDIAN
#define HIGH_WORD(x) (dword_union(x).w[1])
#else
#define HIGH_WORD(x) (dword_union(x).w[0])
#endif

// if the above HIGH_WORD macro doesn't work (if you are not sure, compile it
// and run the validation tests), try this:
// #define HIGH_WORD(x) (word)((x)>>WORD_BITS)

#if defined(_MSC_VER) || defined(__BCPLUSPLUS__)
#define INTEL_INTRINSICS
#define FAST_ROTATE
#elif defined(__MWERKS__) && TARGET_CPU_PPC
#define PPC_INTRINSICS
#define FAST_ROTATE
#elif defined(__GNUC__) && defined(__i386__)
// GCC does peephole optimizations which should result in using rotate instructions
#define FAST_ROTATE
#endif

// can't use std::min or std::max in MSVC60 or Cygwin 1.1.0
template <class _Tp>
inline const _Tp& STDMIN(const _Tp& __a, const _Tp& __b) {
  return __b < __a ? __b : __a;
}

template <class _Tp>
inline const _Tp& STDMAX(const _Tp& __a, const _Tp& __b) {
  return  __a < __b ? __b : __a;
}

#ifdef _MSC_VER
// 4250: dominance
// 4660: explicitly instantiating a class that's already implicitly instantiated
// 4661: no suitable definition provided for explicit template instantiation request
// 4786: identifer was truncated in debug information
// 4355: 'this' : used in base member initializer list
// 4800: converting int to bool
#pragma warning(disable: 4250 4660 4661 4786 4355 4800)
#endif

NAMESPACE_END

#endif
