/*
* Copyright (c) 2021 Apple Inc. All rights reserved.
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

// Implementation of `strtod` and related functions
// Based on version 3 of "ffpp_strtofp"
// Author: Tim Kientzle
//
// This file supports parsing floating-point values from ASCII/UTF8
// text into the following commonly-used floating-point formats:
//  * IEEE 754/ISO 60559 binary16 ("half")
//  * IEEE 754/ISO 60559 binary32 ("float")
//  * IEEE 754/ISO 60559 binary64 ("double")
//  * Intel x87 80-bit extended format ("float80")
//  * IEEE 754/ISO 60559 binary128
//
// The core functionality consists of the following private functions,
// which can be compiled for use on any platform regardless of the
// local floating-point support:
//
//  * _ffpp_strtoencf16_l (see footnote [1])
//  * _ffpp_strtoencf32_l
//  * _ffpp_strtoencf64_l
//  * _ffpp_strtoencf80_l (see footnote [2])
//  * _ffpp_strtoencf128_l
//
// The following standard functions are defined as wrappers around the
// above to provide the APIs defined in the current C standardization
// effort:
//
//  * strtof (ISO C17)
//  * strtof_l (ISO C17)
//  * strtod (ISO C17)
//  * strtod_l (ISO C17)
//  * strtold (ISO C17, see footnote [3])
//  * strtold_l (ISO C17, see footnote [3])
//  * strtoencf16 (TS 18661-3)
//  * strtoencf32 (TS 18661-3)
//  * strtoencf64 (TS 18661-3)
//  * strtoencf64x (TS 18661-3, see footnote [2])
//  * strtoencf128 (TS 18661-3)
//
// TODO: The following wrappers can easily be added for
// any platform that defines the required `_Float##` types.
//  * strtof16 (TS 18661-3)
//  * strtof32 (TS 18661-3)
//  * strtof64 (TS 18661-3)
//  * strtof64x (TS 18661-3, see footnote [2])
//  * strtof128 (TS 18661-3)
//
// IMPLEMENTATION NOTES
// --------------------------------
//
// This is a new implementation that uses ideas from a number of
// sources, including Clinger's 1990 paper, Gay's gdtoa library,
// Lemire's fast_double_parser implementation, Google's abseil
// library, as well as work I've done for the Swift standard library.
//
// All of the parsers use the same initial parsing logic and fall back
// to the same arbitrary-precision integer code.  In between these,
// they use varying format-specific optimizations.
//
// First Step: Initial Parsing
//
// The initial parsing of the textual input is handled by
// `fastParse64`.  As the name suggests, this uses a fixed-size 64-bit
// accumulator for speed and is heavily optimized assuming that the
// input significand has at most 19 digits. Longer input will overflow
// the accumulator, triggering an additional scan of the input.  This
// initial parse also detects Hex floats, nans, and "inf"/"infinity"
// strings and dispatches those to specialized implementations.
//
// With the initial parse complete, the challenge is to compute
//    decimalSignificand * 10^decimalExponent
// with precisely correct rounding as quickly as possible.
//
// Last Step: arbitrary-precision integer calculation (generalSlowpath)
//
// Specific formatters use a variety of optimized paths that provide
// quick results for specific classes of input.  But none of those
// work for every input value.  So we have a final fallback that uses
// arbitrary-precision integer arithmetic to compute the exact results
// with guaranteed accuracy for all inputs.  Of course, the required
// arbitrary-precision arithmetic can be significantly more expensive,
// especially when the significand is very long or the exponent is
// very large.
//
// Two optimizations are worth mentioning:
//
// Powers of 5: We break the power of 10 computation into a power of 5
// and a power of 2.  The latter can be simply folded into the final
// FP exponent, so this effectively reduces the power of 10
// computation to the computation of a power of 5, which is a
// significantly smaller number.  For very large exponents, the run
// time is dominated by this power of 5 computation.  (Up to 95% of
// the CPU time for extreme binary128 values.)
//
// Limit on significand digits: I first saw this optimization in the
// Abseil library.  First, consider the exact decimal expansions for
// all the exact midpoints between adjacent pairs of floating-point
// values.  There is some maximum number of significant digits
// `maxDecimalMidpointDigits`.  Following an argument from Clinger, we
// only need to be able to distinguish whether we are above or below
// any such midpoint.  So it suffices to consider the first
// `maxDecimalMidpointDigits`, appending a single digit that is
// non-zero if the trailing digits are non-zero. This allows us to
// limit the total size of the arithmetic used in this stage.  In
// particular, for double, this limits us to less than 1024 bytes of
// total space, which can easily fit on the stack, allowing us to
// parse any double input regardless of length without allocation.
//
// For binary128, the comparable limit is 11564 digits, which gives a
// maximum work buffer size of nearly 10k.  This seems a bit large for
// the stack, but a buffer of 1536 bytes is big enough to process any
// binary128 with less than 100 digits, regardless of exponent.  TODO:
// For a smaller range of exponents, we can limit
// maxDecimalMidpointDigits further.  That would allow us to process
// any binary128 within a range of exponents regardless of number of
// digits with the same 1536-byte buffer.
//
// Note: Compared to Clinger's AlgorithmR, this requires fewer
// arbitrary-precision operations and gives the correct answer
// directly without requiring a nearly-correct initial value.
// Compared to Clinger's AlgorithmM, this takes advantage of the fact
// that our integer arithmetic is occuring in the same base as used by
// the final FP format.  This means we can interpret the bits from a
// simple calculation instead of doing additional work to abstractly
// compute the target format.
//
// These first and last steps (`fastParse64` and `generalSlowpath`)
// are sufficient to provide guaranteed correct results for any
// format.  The optimizations described next are accelerators that
// allow us to provide a result more quickly for common cases where
// the additional code complexity and testing cost can be justified.
//
// Optimization: Use a single Floating-point calculation
//
// Clinger(1990) observed that when converting to double, if the
// significand and 10^k are both exactly representable by doubles,
// then
//    (double)significand * (double)10^k
// is always correct with a single double multiplication.
// Similarly, if 10^-k is exactly representable, then
//    (double)significand / (double)10^(-k)
// is always correct with a single double division.
//
// In particular, any significand of 15 digits or less can be exactly
// represented by a double, as can any power of 10 up to 10^22.
//
// There are a few similar cases where we can provide exact inputs to
// a single floating-point operation.  Since a single FP operation
// always produces correctly-rounded results (in the current rounding
// mode), these always produce correct results for the corresponding
// range of inputs.  Since this relies on the hardware FPU, it is very
// fast when it can be used.
//
// This optimization works especially well for hand-entered input,
// which typically has few digits and a small exponent.  It works less
// well for JSON, as random double values in JSON are typically
// presented with 16 or 17 digits.  Fast FMA or mixed-precision
// arithmetic can extend this technique further in certain
// environments.  In particular, FPU-supported multiplication and
// division of binary128 arguments with a binary64 result would handle
// the common JSON cases.
//
// Optimization: Interval calculation
//
// We can easily compute fixed-precision upper and lower bounds for
// the power-of-10 value from a lookup table.  Likewise, we can
// construct bounds for an arbitrary-length significand by inspecting
// just the first digits.  From these bounds, we can compute upper and
// lower bounds for the final result, all with fast fixed-precision
// integer arithmetic.  Depending on the precision, these upper and
// lower bounds can coincide for more than 99% of all inputs,
// guaranteeing the correct result in those cases.  This also allows
// us to use fast fixed-precision arithmetic for very long inputs,
// only using the first digits of the significand in cases where the
// additional digits do not affect the result.
//
// PERFORMANCE
// --------------------------------
//
// This strtod is about 10x faster than the implementation used by
// Apple's libc prior to 2022.  On Lemire's `canada.txt` benchmark,
// this implementation achieves over 700MB/s compared to 75MB/s for
// the earlier implementation when parsing a large collection of
// latitude/longitude values expressed as doubles with 15-17 digits.
// That makes it about 70% the speed of Lemire's fast_double_parser
// implementation, but still significantly faster than the other
// implementations benchmarked by Lemire.
//
// Note: You can achieve speed similar to Lemire's implementation by
// judiciously inlining supporting routines, disabling rounding mode
// and locale support, and using a direct lookup into a fully-expanded
// table instead of two table lookups and a multiplication.
//
// For other inputs, performance varies depending on the particular
// optimization that gets used.  Binary128 and Float80 parsing here
// uses a more heavily-factored power-of-10 table that requires up to
// 3 multiplications to produce a lower bound.  Binary16 currently
// has no format-specific optimizations since the arbitrary-precision
// path is fast enough for such a small range of exponents.
//
// CORRECTNESS
// --------------------------------
//
// Primary accuracy testing has relied on a modified form of the
// "FFPP" test suite I developed for work on "SwiftDtoa.c".  This includes
// over 100 million computed test cases.
//
// This strtod has also been fuzz-tested to ensure that the behavior
// matches the previous macOS libc implementation based on gdtoa
// (except for known bugs in that code):
//  * It returns the same double result
//  * It updates `end` identically
//  * It updates `errno` identically
//  * For any locale
//  * For any standard FP rounding mode
//
// NOTES
// --------------------------------
//
// General terminology note: I use "binary##" to refer to the IEEE 754
// portable binary formats, and "float80" to refer to the Intel x87
// 80-bit extended format.
//
// [1] TS18661-3 defines parsing functions that use the current
// default locale, but it does not (as of early 2021) define any
// variants that take an explicit locale argument. In order to provide
// ISO C17 `strtod_l` functionality, I've found it useful to extend
// the TS18661-3 suite of functions with `_l` variants.
//
// [2] The term "binary64x" or "f64x" in various standards refers
// generally to any extended format, not specifically the Intel x87
// 80-bit format.  This makes it difficult to use the term `f64x` for
// portable implementations.  To keep things clear, I have
// `strtoencf80_l` to specifically support the Intel float80 format
// and define `strtoencf64x` as a wrapper that calls into either
// `strtoencf64_l` or `strtoencf80_l` depending on the local system
// conventions (using the same approach as used for `strtold`).
//
// [3] `strtold` is defined as a wrapper for whichever supported
// format (binary64, float80, or binary128) corresponds to long double
// on the local system.  Search below for uses of `LONG_DOUBLE_IS_` to
// see how this is done.
//
//
// TODO:
//
//  * Figure out how to test whether `_Float##` types are provided,
//    use that to define `strtof##` automatically on systems that
//    support TS18661-3.
//
//  * Support big-endian systems. Much of the support for
//    binary16/32/64 should already work correctly since it builds the
//    result bitwise in an unsigned integer and uses memcpy() to copy
//    the bits.  This works if ints and floats have the same
//    endianness, which I believe is universally true on all current
//    hardware.  Other code builds up results assuming a particular
//    byte order and will need to be modified to support big-endian
//    environments.
//

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <langinfo.h>
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

// #pragma STDC FENV_ACCESS ON

// ================================================================
// Detect the floating-point formats supported on this platform
// by testing the standard macros defined in float.h
// ================================================================

// Does "float" on this system use IEEE 754 binary32 format?
// (Almost all modern systems do this.)
#if (FLT_RADIX == 2) && (FLT_MANT_DIG == 24) && (FLT_MIN_EXP == -125) && (FLT_MAX_EXP == 128)
  #define FLOAT_IS_BINARY32 1
#else
  #define FLOAT_IS_BINARY32 0
#endif

// Does "double" on this system use IEEE 754 binary64 format?
// (Almost all modern systems do this.)
#if (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_MIN_EXP == -1021) && (DBL_MAX_EXP == 1024)
  #define DOUBLE_IS_BINARY64 1
#else
  #define DOUBLE_IS_BINARY64 0
#endif

// Does "long double" on this system use IEEE 754 binary64 format?
// (Example: Windows on all hardware, macOS on ARM.)
#if (FLT_RADIX == 2) && (LDBL_MANT_DIG == 53) && (LDBL_MIN_EXP == -1021) && (LDBL_MAX_EXP == 1024)
  #define LONG_DOUBLE_IS_BINARY64 1
#else
  #define LONG_DOUBLE_IS_BINARY64 0
#endif

// Is "long double" on this system the same as Float80?
// (Example: macOS, Linux, and FreeBSD when running on x86 or x86_64 processors.)
#if (FLT_RADIX == 2) && (LDBL_MANT_DIG == 64) && (LDBL_MIN_EXP == -16381) && (LDBL_MAX_EXP == 16384)
 #define LONG_DOUBLE_IS_FLOAT80 1
#else
 #define LONG_DOUBLE_IS_FLOAT80 0
#endif

// Does "long double" on this system use IEEE 754 binary128 format?
// (Example: Android on LP64 hardware.)
#if (FLT_RADIX == 2) && (LDBL_MANT_DIG == 113) && (LDBL_MIN_EXP == -16381) && (LDBL_MAX_EXP == 16384)
 #define LONG_DOUBLE_IS_BINARY128 1
#else
 #define LONG_DOUBLE_IS_BINARY128 0
#endif

// ================================================================
// Detect/configure local platform arithmetic

#if defined(__SIZEOF_INT128__)
  // We get a significant speed boost if we can use the __uint128_t
  // type that's present in GCC and Clang on 64-bit architectures.
  #define HAVE_UINT128_T 1
  #define MP_WORD_BITS 32 // Multi-precision ints use 32-bit words on 64-bit platforms
#else
  #define HAVE_UINT128_T 0
  #define MP_WORD_BITS 16 // Multi-precision ints use 16-bit words on 32-bit platforms
#endif

// ================================================================
// How to get locale-specific decimal point character

#if defined(__linux__)
#define locale_decimal_point(loc) \
  ((const unsigned char *)((loc) == LC_GLOBAL_LOCALE ? nl_langinfo(RADIXCHAR) : nl_langinfo_l(RADIXCHAR, (loc))))
#elif defined(__APPLE__)
#define locale_decimal_point(loc) ((const unsigned char *)(localeconv_l((loc))->decimal_point))
#else
#endif


// ================================================================
//
// Enable/disable particular formats
//
// ================================================================

// Enable binary16/32/64 by default everywhere.
#ifndef ENABLE_BINARY16_SUPPORT
 #define ENABLE_BINARY16_SUPPORT 1
#endif
#ifndef ENABLE_BINARY32_SUPPORT
 #define ENABLE_BINARY32_SUPPORT 1
#endif
#ifndef ENABLE_BINARY64_SUPPORT
 #define ENABLE_BINARY64_SUPPORT 1
#endif
// Enable float80 by default only if necessary to support long double
#ifndef ENABLE_FLOAT80_SUPPORT
 #if LONG_DOUBLE_IS_FLOAT80
  #define ENABLE_FLOAT80_SUPPORT 1
 #else
  #define ENABLE_FLOAT80_SUPPORT 0
 #endif
#endif
// Enable binary128 by default only if necessary to support long double
#ifndef ENABLE_BINARY128_SUPPORT
 #if LONG_DOUBLE_IS_BINARY128
  #define ENABLE_BINARY128_SUPPORT 1
 #else
  #define ENABLE_BINARY128_SUPPORT 0
 #endif
#endif

// Enable float80 interval optimization by default.
// If you want to be able to tick off "float80" support but don't want to
// pay for it, define this to 0.  Then you'll just get the minimum.
// Enabling this costs about 3.5k of code.
// Note: The code is shared with binary128, so there's no point to enabling
// it for one but not the other.
#if ENABLE_FLOAT80_SUPPORT
 #ifndef ENABLE_FLOAT80_OPTIMIZATIONS
  #define ENABLE_FLOAT80_OPTIMIZATIONS 1
 #endif
#else
 #undef ENABLE_FLOAT80_OPTIMIZATIONS
#endif

// Enable binary128 interval optimization by default.
#if ENABLE_BINARY128_SUPPORT
  #ifndef ENABLE_BINARY128_OPTIMIZATIONS
   #define ENABLE_BINARY128_OPTIMIZATIONS 1
 #endif
#else
 #undef ENABLE_BINARY128_OPTIMIZATIONS
#endif

// At least one format must be enabled
#if !ENABLE_BINARY16_SUPPORT && !ENABLE_BINARY32_SUPPORT && !ENABLE_BINARY64_SUPPORT && !ENABLE_FLOAT80_SUPPORT && !ENABLE_BINARY128_SUPPORT
#error At least one format must be enabled
#endif

// ================================================================
// Look up tables

static const unsigned char hexdigit[256] = {
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
     0,    1,    2,    3,     4,    5,    6,    7,
     8,    9, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff,   10,   11,   12,    13,   14,   15, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff,   10,   11,   12,    13,   14,   15, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff
};

// ================================================================
// ================================================================
//
// Power-of-10 tables
//
// ================================================================
// ================================================================


#define binaryExponentFor10ToThe(p) ((int)(((((int64_t)(p)) * 55732705) >> 24) + 1))

// For binary32 only, we need a single 880-byte powersOf10_Float table below

// For binary64 only, we need a 224-byte subset of the first table and
// a 232-byte additional table for a total of 456 bytes

// For both, we can overlap somewhat so we only need the 880-byte binary32
// table and the auxiliary 232-byte table for binary64, for a total of 1112 bytes.

#if ENABLE_BINARY32_SUPPORT || ENABLE_BINARY64_SUPPORT || ENABLE_FLOAT80_OPTIMIZATIONS || ENABLE_BINARY128_OPTIMIZATIONS
// These are the 64-bit binary significands of the
// largest binary floating-point value that is not greater
// than the corresponding power of 10.

// That is, when these are not exact, they are less than
// the exact decimal value.  This allows us to use these
// to construct tight intervals around the true power
// of ten value.
static const uint64_t powersOf10_Float[] = {
#if ENABLE_BINARY32_SUPPORT
    0xb0af48ec79ace837ULL, // x 2^-232 ~= 10^-70
    0xdcdb1b2798182244ULL, // x 2^-229 ~= 10^-69
    0x8a08f0f8bf0f156bULL, // x 2^-225 ~= 10^-68
    0xac8b2d36eed2dac5ULL, // x 2^-222 ~= 10^-67
    0xd7adf884aa879177ULL, // x 2^-219 ~= 10^-66
    0x86ccbb52ea94baeaULL, // x 2^-215 ~= 10^-65
    0xa87fea27a539e9a5ULL, // x 2^-212 ~= 10^-64
    0xd29fe4b18e88640eULL, // x 2^-209 ~= 10^-63
    0x83a3eeeef9153e89ULL, // x 2^-205 ~= 10^-62
    0xa48ceaaab75a8e2bULL, // x 2^-202 ~= 10^-61
    0xcdb02555653131b6ULL, // x 2^-199 ~= 10^-60
    0x808e17555f3ebf11ULL, // x 2^-195 ~= 10^-59
    0xa0b19d2ab70e6ed6ULL, // x 2^-192 ~= 10^-58
    0xc8de047564d20a8bULL, // x 2^-189 ~= 10^-57
    0xfb158592be068d2eULL, // x 2^-186 ~= 10^-56
    0x9ced737bb6c4183dULL, // x 2^-182 ~= 10^-55
    0xc428d05aa4751e4cULL, // x 2^-179 ~= 10^-54
    0xf53304714d9265dfULL, // x 2^-176 ~= 10^-53
    0x993fe2c6d07b7fabULL, // x 2^-172 ~= 10^-52
    0xbf8fdb78849a5f96ULL, // x 2^-169 ~= 10^-51
    0xef73d256a5c0f77cULL, // x 2^-166 ~= 10^-50
    0x95a8637627989aadULL, // x 2^-162 ~= 10^-49
    0xbb127c53b17ec159ULL, // x 2^-159 ~= 10^-48
    0xe9d71b689dde71afULL, // x 2^-156 ~= 10^-47
    0x9226712162ab070dULL, // x 2^-152 ~= 10^-46
    0xb6b00d69bb55c8d1ULL, // x 2^-149 ~= 10^-45
    0xe45c10c42a2b3b05ULL, // x 2^-146 ~= 10^-44
    0x8eb98a7a9a5b04e3ULL, // x 2^-142 ~= 10^-43
    0xb267ed1940f1c61cULL, // x 2^-139 ~= 10^-42
    0xdf01e85f912e37a3ULL, // x 2^-136 ~= 10^-41
    0x8b61313bbabce2c6ULL, // x 2^-132 ~= 10^-40
    0xae397d8aa96c1b77ULL, // x 2^-129 ~= 10^-39
    0xd9c7dced53c72255ULL, // x 2^-126 ~= 10^-38
    0x881cea14545c7575ULL, // x 2^-122 ~= 10^-37
    0xaa242499697392d2ULL, // x 2^-119 ~= 10^-36
    0xd4ad2dbfc3d07787ULL, // x 2^-116 ~= 10^-35
    0x84ec3c97da624ab4ULL, // x 2^-112 ~= 10^-34
    0xa6274bbdd0fadd61ULL, // x 2^-109 ~= 10^-33
    0xcfb11ead453994baULL, // x 2^-106 ~= 10^-32
    0x81ceb32c4b43fcf4ULL, // x 2^-102 ~= 10^-31
    0xa2425ff75e14fc31ULL, // x 2^-99 ~= 10^-30
    0xcad2f7f5359a3b3eULL, // x 2^-96 ~= 10^-29
    0xfd87b5f28300ca0dULL, // x 2^-93 ~= 10^-28
    0x9e74d1b791e07e48ULL, // x 2^-89 ~= 10^-27
    0xc612062576589ddaULL, // x 2^-86 ~= 10^-26
    0xf79687aed3eec551ULL, // x 2^-83 ~= 10^-25
    0x9abe14cd44753b52ULL, // x 2^-79 ~= 10^-24
    0xc16d9a0095928a27ULL, // x 2^-76 ~= 10^-23
    0xf1c90080baf72cb1ULL, // x 2^-73 ~= 10^-22
    0x971da05074da7beeULL, // x 2^-69 ~= 10^-21
    0xbce5086492111aeaULL, // x 2^-66 ~= 10^-20
    0xec1e4a7db69561a5ULL, // x 2^-63 ~= 10^-19
    0x9392ee8e921d5d07ULL, // x 2^-59 ~= 10^-18
    0xb877aa3236a4b449ULL, // x 2^-56 ~= 10^-17
    0xe69594bec44de15bULL, // x 2^-53 ~= 10^-16
    0x901d7cf73ab0acd9ULL, // x 2^-49 ~= 10^-15
    0xb424dc35095cd80fULL, // x 2^-46 ~= 10^-14
    0xe12e13424bb40e13ULL, // x 2^-43 ~= 10^-13
    0x8cbccc096f5088cbULL, // x 2^-39 ~= 10^-12
    0xafebff0bcb24aafeULL, // x 2^-36 ~= 10^-11
    0xdbe6fecebdedd5beULL, // x 2^-33 ~= 10^-10
    0x89705f4136b4a597ULL, // x 2^-29 ~= 10^-9
    0xabcc77118461cefcULL, // x 2^-26 ~= 10^-8
    0xd6bf94d5e57a42bcULL, // x 2^-23 ~= 10^-7
    0x8637bd05af6c69b5ULL, // x 2^-19 ~= 10^-6
    0xa7c5ac471b478423ULL, // x 2^-16 ~= 10^-5
    0xd1b71758e219652bULL, // x 2^-13 ~= 10^-4
    0x83126e978d4fdf3bULL, // x 2^-9 ~= 10^-3
    0xa3d70a3d70a3d70aULL, // x 2^-6 ~= 10^-2
    0xccccccccccccccccULL, // x 2^-3 ~= 10^-1
#endif
    // These values are exact; we use them together with
    // _CoarseBinary64 below for binary64 format.
    0x8000000000000000ULL, // x 2^1 == 10^0 exactly
    0xa000000000000000ULL, // x 2^4 == 10^1 exactly
    0xc800000000000000ULL, // x 2^7 == 10^2 exactly
    0xfa00000000000000ULL, // x 2^10 == 10^3 exactly
    0x9c40000000000000ULL, // x 2^14 == 10^4 exactly
    0xc350000000000000ULL, // x 2^17 == 10^5 exactly
    0xf424000000000000ULL, // x 2^20 == 10^6 exactly
    0x9896800000000000ULL, // x 2^24 == 10^7 exactly
    0xbebc200000000000ULL, // x 2^27 == 10^8 exactly
    0xee6b280000000000ULL, // x 2^30 == 10^9 exactly
    0x9502f90000000000ULL, // x 2^34 == 10^10 exactly
    0xba43b74000000000ULL, // x 2^37 == 10^11 exactly
    0xe8d4a51000000000ULL, // x 2^40 == 10^12 exactly
    0x9184e72a00000000ULL, // x 2^44 == 10^13 exactly
    0xb5e620f480000000ULL, // x 2^47 == 10^14 exactly
    0xe35fa931a0000000ULL, // x 2^50 == 10^15 exactly
    0x8e1bc9bf04000000ULL, // x 2^54 == 10^16 exactly
    0xb1a2bc2ec5000000ULL, // x 2^57 == 10^17 exactly
    0xde0b6b3a76400000ULL, // x 2^60 == 10^18 exactly
    0x8ac7230489e80000ULL, // x 2^64 == 10^19 exactly
    0xad78ebc5ac620000ULL, // x 2^67 == 10^20 exactly
    0xd8d726b7177a8000ULL, // x 2^70 == 10^21 exactly
    0x878678326eac9000ULL, // x 2^74 == 10^22 exactly
    0xa968163f0a57b400ULL, // x 2^77 == 10^23 exactly
    0xd3c21bcecceda100ULL, // x 2^80 == 10^24 exactly
    0x84595161401484a0ULL, // x 2^84 == 10^25 exactly
    0xa56fa5b99019a5c8ULL, // x 2^87 == 10^26 exactly
    0xcecb8f27f4200f3aULL, // x 2^90 == 10^27 exactly
#if ENABLE_BINARY32_SUPPORT
    0x813f3978f8940984ULL, // x 2^94 ~= 10^28
    0xa18f07d736b90be5ULL, // x 2^97 ~= 10^29
    0xc9f2c9cd04674edeULL, // x 2^100 ~= 10^30
    0xfc6f7c4045812296ULL, // x 2^103 ~= 10^31
    0x9dc5ada82b70b59dULL, // x 2^107 ~= 10^32
    0xc5371912364ce305ULL, // x 2^110 ~= 10^33
    0xf684df56c3e01bc6ULL, // x 2^113 ~= 10^34
    0x9a130b963a6c115cULL, // x 2^117 ~= 10^35
    0xc097ce7bc90715b3ULL, // x 2^120 ~= 10^36
    0xf0bdc21abb48db20ULL, // x 2^123 ~= 10^37
    0x96769950b50d88f4ULL, // x 2^127 ~= 10^38
    0xbc143fa4e250eb31ULL, // x 2^130 ~= 10^39
#endif
};
#endif

#if ENABLE_BINARY64_SUPPORT || ENABLE_FLOAT80_OPTIMIZATIONS || ENABLE_BINARY128_OPTIMIZATIONS
 #if ENABLE_BINARY32_SUPPORT
  static const uint64_t *powersOf10_Exact64 = powersOf10_Float + 70;
 #else
  static const uint64_t *powersOf10_Exact64 = powersOf10_Float;
 #endif
#endif

#if ENABLE_BINARY64_SUPPORT
// Rounded-down values supporting the full range of binary64.
// As above, when not exact, these are rounded down to the
// nearest value lower than or equal to the exact power of 10.
//
// Table size: 232 bytes
//
// We only store every 28th power of ten here.
// We can multiply by an exact 64-bit power of
// ten from powersOf10_Exact64 above to reconstruct the
// significand for any power of 10.
static const uint64_t powersOf10_CoarseBinary64[30] = {
    0xdd5a2c3eab3097cbULL, // x 2^-1395 ~= 10^-420
    0xdf82365c497b5453ULL, // x 2^-1302 ~= 10^-392
    0xe1afa13afbd14d6dULL, // x 2^-1209 ~= 10^-364
    0xe3e27a444d8d98b7ULL, // x 2^-1116 ~= 10^-336
    0xe61acf033d1a45dfULL, // x 2^-1023 ~= 10^-308
    0xe858ad248f5c22c9ULL, // x 2^-930 ~= 10^-280
    0xea9c227723ee8bcbULL, // x 2^-837 ~= 10^-252
    0xece53cec4a314ebdULL, // x 2^-744 ~= 10^-224
    0xef340a98172aace4ULL, // x 2^-651 ~= 10^-196
    0xf18899b1bc3f8ca1ULL, // x 2^-558 ~= 10^-168
    0xf3e2f893dec3f126ULL, // x 2^-465 ~= 10^-140
    0xf64335bcf065d37dULL, // x 2^-372 ~= 10^-112
    0xf8a95fcf88747d94ULL, // x 2^-279 ~= 10^-84
    0xfb158592be068d2eULL, // x 2^-186 ~= 10^-56
    0xfd87b5f28300ca0dULL, // x 2^-93 ~= 10^-28
    0x8000000000000000ULL, // x 2^1 == 10^0 exactly
    0x813f3978f8940984ULL, // x 2^94 == 10^28 exactly
    0x82818f1281ed449fULL, // x 2^187 ~= 10^56
    0x83c7088e1aab65dbULL, // x 2^280 ~= 10^84
    0x850fadc09923329eULL, // x 2^373 ~= 10^112
    0x865b86925b9bc5c2ULL, // x 2^466 ~= 10^140
    0x87aa9aff79042286ULL, // x 2^559 ~= 10^168
    0x88fcf317f22241e2ULL, // x 2^652 ~= 10^196
    0x8a5296ffe33cc92fULL, // x 2^745 ~= 10^224
    0x8bab8eefb6409c1aULL, // x 2^838 ~= 10^252
    0x8d07e33455637eb2ULL, // x 2^931 ~= 10^280
    0x8e679c2f5e44ff8fULL, // x 2^1024 ~= 10^308
    0x8fcac257558ee4e6ULL, // x 2^1117 ~= 10^336
    0x91315e37db165aa9ULL, // x 2^1210 ~= 10^364
    0x929b7871de7f22b9ULL, // x 2^1303 ~= 10^392
};
#endif

#if ENABLE_FLOAT80_OPTIMIZATIONS || ENABLE_BINARY128_OPTIMIZATIONS
// Every 56th power of 10 across the range of Float80/Binary128
//
// Table size: 2880 bytes
static const uint64_t powersOf10_Binary128[180] = {
    // Low-order ... high-order
    0x0000000000000000ULL, 0x8000000000000000ULL, // x 2^1 == 10^0 exactly
    0xbff8f10e7a8921a4ULL, 0x82818f1281ed449fULL, // x 2^187 ~= 10^56
    0x03e2cf6bc604ddb0ULL, 0x850fadc09923329eULL, // x 2^373 ~= 10^112
    0x90fb44d2f05d0842ULL, 0x87aa9aff79042286ULL, // x 2^559 ~= 10^168
    0x82bd6b70d99aaa6fULL, 0x8a5296ffe33cc92fULL, // x 2^745 ~= 10^224
    0xdb0b487b6423e1e8ULL, 0x8d07e33455637eb2ULL, // x 2^931 ~= 10^280
    0x213a4f0aa5e8a7b1ULL, 0x8fcac257558ee4e6ULL, // x 2^1117 ~= 10^336
    0x1c306f5d1b0b5fdfULL, 0x929b7871de7f22b9ULL, // x 2^1303 ~= 10^392
    0xa7ea9c8838ce9437ULL, 0x957a4ae1ebf7f3d3ULL, // x 2^1489 ~= 10^448
    0xbf1d49cacccd5e68ULL, 0x9867806127ece4f4ULL, // x 2^1675 ~= 10^504
    0x655494c5c95d77f2ULL, 0x9b63610bb9243e46ULL, // x 2^1861 ~= 10^560
    0x02e008393fd60b55ULL, 0x9e6e366733f85561ULL, // x 2^2047 ~= 10^616
    0x55e04dba4b3bd4ddULL, 0xa1884b69ade24964ULL, // x 2^2233 ~= 10^672
    0x44b222741eb1ebbfULL, 0xa4b1ec80f47c84adULL, // x 2^2419 ~= 10^728
    0x1cf4a5c3bc09fa6fULL, 0xa7eb6799e8aec999ULL, // x 2^2605 ~= 10^784
    0x3c4a575151b294dcULL, 0xab350c27feb90accULL, // x 2^2791 ~= 10^840
    0x870a8d87239d8f35ULL, 0xae8f2b2ce3d5dbe9ULL, // x 2^2977 ~= 10^896
    0xdd929f09c3eff5acULL, 0xb1fa17404a30e5e8ULL, // x 2^3163 ~= 10^952
    0x1931b583a9431d7eULL, 0xb5762497dbf17a9eULL, // x 2^3349 ~= 10^1008
    0xe30db03e0f8dd286ULL, 0xb903a90f561d25e2ULL, // x 2^3535 ~= 10^1064
    0x9eb5cb19647508c5ULL, 0xbca2fc30cc19f090ULL, // x 2^3721 ~= 10^1120
    0x24bd4c00042ad125ULL, 0xc054773d149bf26bULL, // x 2^3907 ~= 10^1176
    0x7ea30dbd7ea479e3ULL, 0xc418753460cdcca9ULL, // x 2^4093 ~= 10^1232
    0x764f4cf916b4deceULL, 0xc7ef52defe87b751ULL, // x 2^4279 ~= 10^1288
    0xbeb7fbdc1cbe8b37ULL, 0xcbd96ed6466cf081ULL, // x 2^4465 ~= 10^1344
    0xdce472c619aa3f63ULL, 0xcfd7298db6cb9672ULL, // x 2^4651 ~= 10^1400
    0xe47defc14a406e4fULL, 0xd3e8e55c3c1f43d0ULL, // x 2^4837 ~= 10^1456
    0xb7157c60a24a0569ULL, 0xd80f0685a81b2a81ULL, // x 2^5023 ~= 10^1512
    0xfb0b98f6bbc4f0cbULL, 0xdc49f3445824e360ULL, // x 2^5209 ~= 10^1568
    0xc6c6c1764e047e15ULL, 0xe09a13d30c2dba62ULL, // x 2^5395 ~= 10^1624
    0x87e8dcfc09dbc33aULL, 0xe4ffd276eedce658ULL, // x 2^5581 ~= 10^1680
    0xb1a3642a8da3cf4fULL, 0xe97b9b89d001dab3ULL, // x 2^5767 ~= 10^1736
    0x2d4070f33b21ab7bULL, 0xee0ddd84924ab88cULL, // x 2^5953 ~= 10^1792
    0xa2bf0c63a814e04eULL, 0xf2b70909cd3fd35cULL, // x 2^6139 ~= 10^1848
    0x08f13995cf9c2747ULL, 0xf77790f0a48a45ceULL, // x 2^6325 ~= 10^1904
    0x7a37993eb21444faULL, 0xfc4fea4fd590b40aULL, // x 2^6511 ~= 10^1960
    0xb7b1ada9cdeba84dULL, 0x80a046447e3d49f1ULL, // x 2^6698 ~= 10^2016
    0x0cc6866c5d69b2cbULL, 0x8324f8aa08d7d411ULL, // x 2^6884 ~= 10^2072
    0x7fe2b4308dcbf1a3ULL, 0x85b64a659077660eULL, // x 2^7070 ~= 10^2128
    0x1d73ef3eaac3c964ULL, 0x88547abb1d8e5bd9ULL, // x 2^7256 ~= 10^2184
    0x1e34291b1ef566c7ULL, 0x8affca2bd1f88549ULL, // x 2^7442 ~= 10^2240
    0x9e9383d73d486881ULL, 0x8db87a7c1e56d873ULL, // x 2^7628 ~= 10^2296
    0x9cc5ee51962c011aULL, 0x907eceba168949b3ULL, // x 2^7814 ~= 10^2352
    0x413407cfeeac9743ULL, 0x93530b43e5e2c129ULL, // x 2^8000 ~= 10^2408
    0x7efa7d29c44e11b7ULL, 0x963575ce63b6332dULL, // x 2^8186 ~= 10^2464
    0x5a848859645d1c6fULL, 0x9926556bc8defe43ULL, // x 2^8372 ~= 10^2520
    0x51edea897b34601fULL, 0x9c25f29286e9ddb6ULL, // x 2^8558 ~= 10^2576
    0xb50008d92529e91fULL, 0x9f3497244186fca4ULL, // x 2^8744 ~= 10^2632
    0xf09e780bcc8238d9ULL, 0xa2528e74eaf101fcULL, // x 2^8930 ~= 10^2688
    0x3a5828869701a165ULL, 0xa580255203f84b47ULL, // x 2^9116 ~= 10^2744
    0x8b231a70eb5444ceULL, 0xa8bdaa0a0064fa44ULL, // x 2^9302 ~= 10^2800
    0xfa1bde1f473556a4ULL, 0xac0b6c73d065f8ccULL, // x 2^9488 ~= 10^2856
    0x7730e00421da4d55ULL, 0xaf69bdf68fc6a740ULL, // x 2^9674 ~= 10^2912
    0x7f959cb702329d14ULL, 0xb2d8f1915ba88ca5ULL, // x 2^9860 ~= 10^2968
    0x40c3a071220f5567ULL, 0xb6595be34f821493ULL, // x 2^10046 ~= 10^3024
    0x11c48d02b8326bd3ULL, 0xb9eb5333aa272e9bULL, // x 2^10232 ~= 10^3080
    0x566765461bd2f61bULL, 0xbd8f2f7a1ba47d6dULL, // x 2^10418 ~= 10^3136
    0xb889018e4f6e9a52ULL, 0xc1454a673cb9b1ceULL, // x 2^10604 ~= 10^3192
    0xf85333a94848659fULL, 0xc50dff6d30c3aefcULL, // x 2^10790 ~= 10^3248
    0x1a1aeae7cf8a9d3dULL, 0xc8e9abc872eb2bc1ULL, // x 2^10976 ~= 10^3304
    0x12e29f09d9061609ULL, 0xccd8ae88cf70ad84ULL, // x 2^11162 ~= 10^3360
    0xdf7601457ca20b35ULL, 0xd0db689a89f2f9b1ULL, // x 2^11348 ~= 10^3416
    0xcbdcd02f23cc7690ULL, 0xd4f23ccfb1916df5ULL, // x 2^11534 ~= 10^3472
    0x44289dd21b589d7aULL, 0xd91d8fe9a3d019ccULL, // x 2^11720 ~= 10^3528
    0x95aa118ec1d08317ULL, 0xdd5dc8a2bf27f3f7ULL, // x 2^11906 ~= 10^3584
    0x72c4d2cad73b0a7bULL, 0xe1b34fb846321d04ULL, // x 2^12092 ~= 10^3640
    0xe20a88f1134f906dULL, 0xe61e8ff47461cda9ULL, // x 2^12278 ~= 10^3696
    0xc7c91d5c341ed39dULL, 0xea9ff638c54554e1ULL, // x 2^12464 ~= 10^3752
    0xf659ede2159a45ecULL, 0xef37f1886f4b6690ULL, // x 2^12650 ~= 10^3808
    0x78d946bab954b82fULL, 0xf3e6f313130ef0efULL, // x 2^12836 ~= 10^3864
    0xc9b1474d8f89c269ULL, 0xf8ad6e3fa030bd15ULL, // x 2^13022 ~= 10^3920
    0x6b1d2745340e7b14ULL, 0xfd8bd8b770cb469eULL, // x 2^13208 ~= 10^3976
    0xf22e502fcdd4bca2ULL, 0x81415538ce493bd5ULL, // x 2^13395 ~= 10^4032
    0x7c1735fc3b813c8cULL, 0x83c92edf425b292dULL, // x 2^13581 ~= 10^4088
    0x0367500a8e9a178fULL, 0x865db7a9ccd2839eULL, // x 2^13767 ~= 10^4144
    0xc9ac50475e25293aULL, 0x88ff2f2bade74531ULL, // x 2^13953 ~= 10^4200
    0x0879b2e5f6ee8b1cULL, 0x8badd636cc48b341ULL, // x 2^14139 ~= 10^4256
    0x2f33c652bd12fab7ULL, 0x8e69eee1f23f2be5ULL, // x 2^14325 ~= 10^4312
    0xad6a6308a8e8b557ULL, 0x9133bc8f2a130fe5ULL, // x 2^14511 ~= 10^4368
    0x9dbaa465efe141a0ULL, 0x940b83f23a55842aULL, // x 2^14697 ~= 10^4424
    0x888c9ab2fc5b3437ULL, 0x96f18b1742aad751ULL, // x 2^14883 ~= 10^4480
    0xba00864671d1053fULL, 0x99e6196979b978f1ULL, // x 2^15069 ~= 10^4536
    0x61d59d402aae4feaULL, 0x9ce977ba0ce3a0bdULL, // x 2^15255 ~= 10^4592
    0x803c1cd864033781ULL, 0x9ffbf04722750449ULL, // x 2^15441 ~= 10^4648
    0xa28a151725a55e10ULL, 0xa31dcec2fef14b30ULL, // x 2^15627 ~= 10^4704
    0x5b8452af2302fe13ULL, 0xa64f605b4e3352cdULL, // x 2^15813 ~= 10^4760
    0x82b84cabc828bf93ULL, 0xa990f3c09110c544ULL, // x 2^15999 ~= 10^4816
    0x8d29dd5122e4278dULL, 0xace2d92db0390b59ULL, // x 2^16185 ~= 10^4872
    0x58f8fde02c03a6c6ULL, 0xb045626fb50a35e7ULL, // x 2^16371 ~= 10^4928
    0xd950102978dbd0ffULL, 0xb3b8e2eda91a232dULL, // x 2^16557 ~= 10^4984
};
#endif

// ================================================================
// ================================================================
//
// Power-of-5 tables
//
// ================================================================
// ================================================================

// Integer powers of 5 used to drive the power-of-5 calculator used
// in the slow path
const static uint64_t powersOfFive[] = {1, 5, 25, 125, 625, 3125, 15625,
  78125, 390625, 1953125, 9765625, 48828125UL, 244140625UL, 1220703125UL,
  6103515625ULL, 30517578125ULL, 152587890625ULL, 762939453125ULL,
  3814697265625ULL, 19073486328125ULL, 95367431640625ULL,
  476837158203125ULL, 2384185791015625ULL, 11920928955078125ULL,
  59604644775390625ULL, 298023223876953125ULL, 1490116119384765625ULL,
  7450580596923828125ULL
};

//
// Large integer powers of 5.  These are used to accelerate the
// power-of-5 calculator when computing very large powers needed for
// float80 and binary128 in the slow path.
//
// TODO: Consider dropping these.  If float80/binary128 optimizations
// are enabled, then the interval optimization removes a lot of the
// need for these.  (I've already disabled these to save code for
// anyone that prefers code size to performance for these formats.)

#if ENABLE_FLOAT80_OPTIMIZATIONS || ENABLE_BINARY128_OPTIMIZATIONS
// Useful python snippet to generate these constants:
// a = hex(5 ** 2000)[2:]
// while a:
//   print("0x" + a[-8:] + "UL,")
//   a = a[:-8]

#if MP_WORD_BITS == 32
// Broken down into 32-bit words for use on 64-bit processors.
// Stored from least-significant to most-significant.

const static uint32_t fiveToThe1000[] = {
  0xe76e2661UL, 0xa75f463cUL, 0x8f9ac297UL, 0xb426e5d5UL, 0x8021f455UL,
  0x484b538eUL, 0xd39bcb19UL, 0x32c52790UL, 0x69805e7cUL, 0x78612ddbUL,
  0x84a48774UL, 0x4b7ad105UL, 0xa1bca9a9UL, 0x16d4cc76UL, 0x1b3bc79dUL,
  0x22c38759UL, 0x4ec6864aUL, 0x670dbd77UL, 0xa93ad95bUL, 0xfd07b514UL,
  0x185d4af8UL, 0x491981abUL, 0x7f6e513bUL, 0x1a8298bfUL, 0xccdd06fcUL,
  0x9c225999UL, 0x7a000d77UL, 0x211ecc86UL, 0x678b85c6UL, 0xccb243d3UL,
  0x8c2fb72dUL, 0xf95cada8UL, 0x9c82b396UL, 0x3ef53cd3UL, 0x9093cb88UL,
  0xb5aff7d4UL, 0x7da0bc3eUL, 0x522aea78UL, 0xa0165d55UL, 0x3b34e560UL,
  0x80a282e5UL, 0xf0c08162UL, 0x9d7e4698UL, 0xe0fe7772UL, 0x3d9d710eUL,
  0x8d98ec4eUL, 0xb5954022UL, 0xa0e542c3UL, 0x3be09b67UL, 0x5da7d253UL,
  0x86949f98UL, 0x68641b9eUL, 0xaa166a05UL, 0xdfb84e6bUL, 0x2955510aUL,
  0xe62bf617UL, 0xe8c18fc6UL, 0x8afb443aUL, 0x22f12a84UL, 0x048f59a7UL,
  0xaf620341UL, 0xd377548eUL, 0xefd7f15cUL, 0x2f1121d5UL, 0x9aa2146bUL,
  0xf36453b5UL, 0xc7a3e8ecUL, 0xcab2ae32UL, 0xdb4abe87UL, 0x5a54e6f2UL,
  0xb015e34aUL, 0xc7e774f6UL, 0x3ce36UL,
};

#elif MP_WORD_BITS == 16

// Broken down into 16-bit words for use on 32-bit processors.
// Stored from least-significant to most-significant.
const static uint16_t fiveToThe1000[] = {
  0x2661U, 0xe76eU, 0x463cU, 0xa75fU, 0xc297U, 0x8f9aU, 0xe5d5U,
  0xb426U, 0xf455U, 0x8021U, 0x538eU, 0x484bU, 0xcb19U, 0xd39bU,
  0x2790U, 0x32c5U, 0x5e7cU, 0x6980U, 0x2ddbU, 0x7861U, 0x8774U,
  0x84a4U, 0xd105U, 0x4b7aU, 0xa9a9U, 0xa1bcU, 0xcc76U, 0x16d4U,
  0xc79dU, 0x1b3bU, 0x8759U, 0x22c3U, 0x864aU, 0x4ec6U, 0xbd77U,
  0x670dU, 0xd95bU, 0xa93aU, 0xb514U, 0xfd07U, 0x4af8U, 0x185dU,
  0x81abU, 0x4919U, 0x513bU, 0x7f6eU, 0x98bfU, 0x1a82U, 0x06fcU,
  0xccddU, 0x5999U, 0x9c22U, 0x0d77U, 0x7a00U, 0xcc86U, 0x211eU,
  0x85c6U, 0x678bU, 0x43d3U, 0xccb2U, 0xb72dU, 0x8c2fU, 0xada8U,
  0xf95cU, 0xb396U, 0x9c82U, 0x3cd3U, 0x3ef5U, 0xcb88U, 0x9093U,
  0xf7d4U, 0xb5afU, 0xbc3eU, 0x7da0U, 0xea78U, 0x522aU, 0x5d55U,
  0xa016U, 0xe560U, 0x3b34U, 0x82e5U, 0x80a2U, 0x8162U, 0xf0c0U,
  0x4698U, 0x9d7eU, 0x7772U, 0xe0feU, 0x710eU, 0x3d9dU, 0xec4eU,
  0x8d98U, 0x4022U, 0xb595U, 0x42c3U, 0xa0e5U, 0x9b67U, 0x3be0U,
  0xd253U, 0x5da7U, 0x9f98U, 0x8694U, 0x1b9eU, 0x6864U, 0x6a05U,
  0xaa16U, 0x4e6bU, 0xdfb8U, 0x510aU, 0x2955U, 0xf617U, 0xe62bU,
  0x8fc6U, 0xe8c1U, 0x443aU, 0x8afbU, 0x2a84U, 0x22f1U, 0x59a7U,
  0x048fU, 0x0341U, 0xaf62U, 0x548eU, 0xd377U, 0xf15cU, 0xefd7U,
  0x21d5U, 0x2f11U, 0x146bU, 0x9aa2U, 0x53b5U, 0xf364U, 0xe8ecU,
  0xc7a3U, 0xae32U, 0xcab2U, 0xbe87U, 0xdb4aU, 0xe6f2U, 0x5a54U,
  0xe34aU, 0xb015U, 0x74f6U, 0xc7e7U, 0xce36U, 0x3U,
};
#endif
#endif

// ================================================================
// ================================================================
//
// Fixed-width integer routines
//
// ================================================================
// ================================================================

#if HAVE_UINT128_T
#define multiply64x64RoundingDown(lhs, rhs) (((__uint128_t)(lhs) * (rhs)) >> 64)
#else
static uint64_t multiply64x64RoundingDown(uint64_t lhs, uint64_t rhs) {
  uint64_t a = (lhs >> 32) * (rhs >> 32);
  uint64_t b = (lhs >> 32) * (rhs & UINT32_MAX);
  uint64_t c = (lhs & UINT32_MAX) * (rhs >> 32);
  uint64_t d = (lhs & UINT32_MAX) * (rhs & UINT32_MAX);
  b += (c & UINT32_MAX) + (d >> 32);
  return a + (b >> 32) + (c >> 32);
}
#endif

// ================================================================
// ================================================================
//
// Multi-Precision Integer Routines
//
// ================================================================
// ================================================================

// Configure our multi-precision integer machinery with appropriate types
#if MP_WORD_BITS == 16
typedef uint16_t mp_word_t; // Type of a single word in an MP integer
typedef uint32_t mp_dword_t; // Type big enough to hold a two-word MP integer
static const mp_word_t MP_WORD_MAX = UINT16_MAX;
#elif MP_WORD_BITS == 32
typedef uint32_t mp_word_t; // Type of a single word in an MP integer
typedef uint64_t mp_dword_t; // Type big enough to hold a two-word MP integer
static const mp_word_t MP_WORD_MAX = UINT32_MAX;
#else
#error MP_WORD_BITS undefined
#endif
static const int mp_word_bits = sizeof(mp_word_t) * 8;
// __builtin_clz() takes an `unsigned` which may be different from mp_word_t
// This adjusts the result accordingly.
#define CLZ_WORD(word) (__builtin_clz((word)) + (mp_word_bits - sizeof(unsigned) * 8))

// A multi-precision integer is represented as two pointers:
// lsw - points to least-significant word (lowest address in memory)
// msw - points to one beyond the most-significant word
// In particular, the following are true:
//  number of words == msw - lsw
//  mp is empty/zero if msw == lsw
//  lsw[0] is least-significant word
//  lsw[1] is second-least-significant word
//  ...
//  msw[-2] is second-most-significant word
//  msw[-1] is most-significant word

// Caveat: The MP routines in this file are optimized to just extend
// the value in-place as necessary without any checking to see whether
// the allocated memory is sufficient.  That requires that the original
// allocation be sufficient for the largest value that will occur.
typedef struct { mp_word_t *lsw; mp_word_t *msw; } mp_t;

// Shift the mpint left by the indicated number of
// bits.  This will extend the mpint at the msw end
// as necessary.
static void shiftLeftMP(mp_t *work, int shift) {
  int wordsShift = shift / mp_word_bits;
  int bitsShift = shift % mp_word_bits;

  if (wordsShift > 0) {
    memmove(work->lsw + wordsShift,
            work->lsw,
            (work->msw - work->lsw) * sizeof(mp_word_t));
    memset(work->lsw, 0, wordsShift * sizeof(mp_word_t));
    work->msw += wordsShift;
  }

  if (bitsShift > 0) {
    mp_dword_t t = 0;
    mp_word_t *p = work->lsw;
    for (; p < work->msw; p++) {
      t |= (mp_dword_t)*p << bitsShift;
      *p = (mp_word_t)t;
      t >>= mp_word_bits;
    }
    if (t != 0) {
      *p = (mp_word_t)t;
      work->msw = p + 1;
    }
  }
}

// Return the index of the most-significant set bit
static int bitCountMP(mp_t work) {
  if (work.msw == work.lsw) {
    return 0;
  }
  assert(work.msw[-1] != 0); // High-order word cannot be zero
  return (int)(mp_word_bits - CLZ_WORD(work.msw[-1])) // Bits in high-order word
    + (int)(mp_word_bits * (work.msw - work.lsw - 1)); // Bits in all other words
}

// Add small integer to varint
static void addToMP(mp_t *work, uint64_t addend) {
  uint64_t t = addend;
  mp_word_t *p = work->lsw;
  while (t > 0 && p < work->msw) {
    t += *p;
    *p = (mp_word_t)t;
    t >>= mp_word_bits;
    p += 1;
  }
  while (t > 0) {
    *p = (mp_word_t)t;
    t >>= mp_word_bits;
    p += 1;
  }
  if (p > work->msw) {
    work->msw = p;
  }
}

// Shift an MP right, rounding the result according to
// the current FP rounding mode.
static mp_t shiftRightMPWithRounding(mp_t work,
                                     int shift,
                                     int trailingNonZero,
                                     int negative,
                                     int roundingMode) {
  if (shift == 0) {
    return work;
  }
  if (shift < 0) {
    shiftLeftMP(&work, -shift);
    return work;
  }

  mp_t result = work;
  int wordsShift = shift / mp_word_bits;
  int bitsShift = shift % mp_word_bits;

  if (bitsShift == 0) {
    // We don't really need to shift anything, just drop the low-order
    // words and possibly increment.
    result.lsw += wordsShift;
    switch (roundingMode) {
    case FE_TOWARDZERO:
      return result;
    case FE_DOWNWARD:
      // Upwards & downwards rounding are symmetric
      negative = !negative;
      // FALL THROUGH
    case FE_UPWARD:
      for (mp_word_t *p = work.lsw; p < result.lsw; p++) {
        trailingNonZero |= *p;
      }
      if (negative || !trailingNonZero) {
        return result;
      } else {
        break; // Increment and return
      }
    case FE_TONEAREST:
    default: {
      // shift is non-zero, so result.lsw[-1] is valid
      // and is the most-significant-word of the fraction:
      mp_word_t fractionMsw = result.lsw[-1];
      mp_word_t oneHalf = 1 << (mp_word_bits - 1);
      if (fractionMsw < oneHalf) {
        return result;
      } else if (fractionMsw > oneHalf) {
        break; // Increment and return
      } else {
        for (mp_word_t *p = work.lsw; p < result.lsw - 1; p++) {
          trailingNonZero |= *p;
        }
        if (trailingNonZero || ((result.msw > result.lsw) && (result.lsw[0] & 1))) {
          break; // Increment and return
        }
        return result;
      }
    }
    }
    // Increment and return
    addToMP(&result, 1);
    return result;
  }

  result.lsw += wordsShift;
  mp_word_t fraction = result.lsw[0] & ((1 << bitsShift) - 1);

  mp_word_t *p = result.lsw;
  mp_dword_t t = *p++ >> bitsShift;
  for (; p < result.msw; p++) {
    t |= (mp_dword_t)*p << (mp_word_bits - bitsShift);
    p[-1] = (mp_word_t)t;
    t >>= mp_word_bits;
  }
  if (t == 0) {
    result.msw -= 1;
  } else {
    p[-1] = (mp_word_t)t;
  }

  switch (roundingMode) {
  case FE_TOWARDZERO:
    return result;
  case FE_DOWNWARD:
    // Upwards & downwards rounding are symmetric
    negative = !negative;
    // FALL THROUGH
  case FE_UPWARD:
    trailingNonZero |= fraction;
    for (mp_word_t *p = work.lsw; p < result.lsw; p++) {
      trailingNonZero |= *p;
    }
    if (negative || !trailingNonZero) {
      return result;
    } else {
      break; // Increment and return
    }
  case FE_TONEAREST:
  default: {
    mp_word_t half = 1 << (bitsShift - 1);
    if (fraction < half) {
      return result;
    } else if (fraction > half) {
      break; // Increment and return
    } else { // First part of fraction is exact half..
      for (mp_word_t *p = work.lsw; p < result.lsw; p++) {
        trailingNonZero |= *p;
      }
      if (trailingNonZero || ((result.msw > result.lsw) && (result.lsw[0] & 1))) {
        break; // Increment and return
      } else {
        return result;
      }
    }
  }
  }
  // Increment and return
  addToMP(&result, 1);
  return result;
}

static mp_t shiftRightMPWithTruncation(mp_t work,
                                       int shift) {
  return shiftRightMPWithRounding(work, shift,
                                  0, 0, FE_TOWARDZERO);
}

// Multiply varint by N
static void multiplyMPByN(mp_t *work, uint32_t n) {
  uint64_t t = 0;
  for (mp_word_t *p = work->lsw; p < work->msw; p++) {
    t += *p * (uint64_t)n;
    *p = (mp_word_t)t;
    t >>= mp_word_bits;
  }
  while (t > 0) {
    work->msw[0] = (mp_word_t)t;
    t >>= mp_word_bits;
    work->msw += 1;
  }
}

static void multiplyByFiveToTheN(mp_t *dest, int power) {
#if HAVE_UINT128_T
  // 128-bit arithmetic lets us multiply 32-bit words by 5^40 (93 bits).
  // For a double, this can loop up to 8 times.
  // Without the large-power optimization in fiveToTheN, this can
  // loop > 100 times for a binary128.  :-/
  while (power > 40) {
    static const uint64_t fiveToThe20 = 95367431640625ULL;
    static const __uint128_t fiveToThe40 = (__uint128_t)fiveToThe20 * fiveToThe20;
    __uint128_t t = 0;
    mp_word_t *p = dest->lsw;
    while (p < dest->msw) {
      t += *p * fiveToThe40;
      *p++ = (mp_word_t)t;
      t >>= mp_word_bits;
    }
    while (t > 0) {
      *p++ = t;
      t >>= mp_word_bits;
    }
    dest->msw = p;
    power -= 40;
  }
#endif

  while (power > 0) {
#if MP_WORD_BITS == 16
    // Limit to 5^20 (47 bits) so we don't overflow
    // a 64-bit accumulator.
    const static int maxPower = 13; // For 64-bit
    uint64_t t = 0;
    uint64_t powerOfFive = powersOfFive[power > maxPower ? maxPower : power];
#elif MP_WORD_BITS == 32
    // With a 128-bit accumulator, we can use the full table
    // of 64-bit powers (up to 5^27).
    const static int maxPower = 27;
    __uint128_t t = 0;
    __uint128_t powerOfFive = powersOfFive[power > maxPower ? maxPower : power];
#endif
    mp_word_t *p = dest->lsw;
    while (p < dest->msw) {
      t += *p * powerOfFive;
      *p++ = (mp_word_t)t;
      t >>= mp_word_bits;
    }
    while (t > 0) {
      *p++ = (mp_word_t)t;
      t >>= mp_word_bits;
    }
    dest->msw = p;
    power -= maxPower;
  }
}

// Compute 5^N
// This is _THE_ performance-critical function for the slow path
// when handling float80 or binary128 with large exponents.
// (Of course, that only applies to the few percent of inputs
// that don't get handled by the optimized paths.)
static void fiveToTheN(mp_t *dest, int power) {

#if ENABLE_FLOAT80_OPTIMIZATIONS || ENABLE_BINARY128_OPTIMIZATIONS
  // Accelerate a very large power with a pre-computed initial value
  // Only for float80/binary128 since binary64 only goes up to 10^325
  if (power >= 1000) {
    memcpy(dest->lsw, fiveToThe1000, sizeof(fiveToThe1000));
    dest->msw = dest->lsw + sizeof(fiveToThe1000) / sizeof(fiveToThe1000[0]);
    power -= 1000;
  }
  else
#endif
  {
    // Initialize with as large a power of 5 as we can from the standard table
    const static int maxTablePower = sizeof(powersOfFive) / sizeof(powersOfFive[0]) - 1;
    int thisPower = power > maxTablePower ? maxTablePower : power;
    uint64_t t = powersOfFive[thisPower];
    mp_word_t *p = dest->lsw;
    while (t > 0) {
      *p++ = (mp_word_t)t;
      t >>= mp_word_bits;
    }
    dest->msw = p;
    power -= thisPower;
  }

  multiplyByFiveToTheN(dest, power);
}

// Following "Algorithm D" from Knuth AOCP Section 4.3.1
// Accepts:
//   Numerator
//   Denominator
//   *nonZeroRemainder: integer to hold remainder status
// Returns:
//   quotient stored in numerator area
//   numerator is destroyed
//   *nonZeroRemainder set to non-zero iff remainder was non-zero
static mp_t divideMPByMP(mp_t numerator, mp_t denominator, int *nonZeroRemainder) {
  // Make sure we haven't picked up a leading zero word anywhere...
  assert(numerator.msw > numerator.lsw);
  assert(denominator.msw > denominator.lsw);
  assert(numerator.msw[-1] != 0);
  assert(denominator.msw[-1] != 0);

  // Full long division algorithm assumes denominator is more than 1 word,
  // so we need to handle 1-word case separately.
  if (denominator.msw - denominator.lsw == 1) {
    mp_dword_t n = denominator.lsw[0];
    mp_dword_t t = 0;
    mp_word_t *p = numerator.msw;
    while (p > numerator.lsw) {
      p -= 1;
      t <<= mp_word_bits;
      t += *p;
      mp_word_t q0 = (mp_word_t)(t / n);
      *p = q0;
      t -= q0 * n;
    }
    *nonZeroRemainder = (t != 0);
    while (numerator.msw[-1] == 0) {
      numerator.msw -= 1;
    }
    return numerator;
  }

  // D1. Normalize: Multiply numerator and denominator by a power of 2
  // so that denominator has the most significant bit set in the
  // most significant word.  This guarantees that qhat below
  // will always be very good.
  int shift = (int)CLZ_WORD(denominator.msw[-1]);
  shiftLeftMP(&denominator, shift);
  shiftLeftMP(&numerator, shift);

  // Add a high-order word to the numerator if necessary
  if (numerator.msw[-1] >= denominator.msw[-1]) {
    numerator.msw[0] = 0;
    numerator.msw += 1;
  }

  // D2. Iterate
  // Numerator and denominator must not be immediately adjacent in
  // memory, since we need an extra word for the quotient to fit in.
  // TODO: Rearrange this so that the quotient can exactly overlay
  // the numerator instead of going one word beyond.  The requirement
  // for an empty word beyond the numerator is non-obvious and easy
  // to screw up.
  assert((numerator.msw < denominator.lsw) || (denominator.msw < numerator.lsw));
  mp_t quotient = {numerator.msw + 1, numerator.msw + 1}; // Quotient will overwrite numerator
  int iterations = (int)((numerator.msw - numerator.lsw) - (denominator.msw - denominator.lsw));
  for (int j = 0; j < iterations; ++j) {
    // D3. Trial division of high-order bits
    mp_word_t qhat;
    mp_dword_t numerator2 =
      ((mp_dword_t)numerator.msw[-1] << mp_word_bits)
      + numerator.msw[-2];
    if (numerator.msw[-1] == denominator.msw[-1]) {
      qhat = MP_WORD_MAX;
    } else {
      qhat = (mp_word_t)(numerator2 / denominator.msw[-1]);
    }
    while (1) {
      mp_dword_t r = numerator2 - qhat * (mp_dword_t)denominator.msw[-1];
      if (r <= MP_WORD_MAX
          && ((denominator.msw[-2] * (mp_dword_t)qhat) > (numerator.msw[-3] + (r << mp_word_bits)))) {
        qhat -= 1;
      } else {
        break;
      }
    }

    // D4. numerator -= qhat * denominator
    mp_dword_t t = 0;
    for (mp_word_t *den = denominator.lsw,
           *num = numerator.msw - (denominator.msw - denominator.lsw) - 1;
         den < denominator.msw;
         num += 1, den += 1) {
      t += qhat * (mp_dword_t)*den;
      unsigned borrow = *num < (mp_word_t)t;
      *num -= (mp_word_t)t;
      t >>= mp_word_bits;
      t += borrow;
    }

    // D5/D6. qhat may have been one too high; if so, correct for that
    // Per Knuth, this happens very infrequently
    if (numerator.msw[-1] < t) {
      qhat -= 1;
      t = 0;
      for (mp_word_t *den = denominator.lsw,
             *num = numerator.msw - (denominator.msw - denominator.lsw) - 1;
           den < denominator.msw;
           num += 1, den += 1) {
        t += *num + (mp_dword_t)*den;
        *num = (mp_word_t)t;
        t >>= mp_word_bits;
      }
    }
    --quotient.lsw;
    *quotient.lsw = qhat;

    // D7. Iterate
    numerator.msw -= 1;
  }

  // D8. Post-process the remainder
  mp_word_t remainderHash = 0;
  for (mp_word_t *num = numerator.lsw; num < numerator.msw; ++num) {
    remainderHash |= *num;
  }
  *nonZeroRemainder = remainderHash != 0;

  while (quotient.msw[-1] == 0) {
    quotient.msw -= 1;
  }
  return quotient;
}

// ================================================================
// ================================================================
//
// Parse state
//
// ================================================================
// ================================================================

// To reduce argument-passing overhead, we store all the state
// in this struct and pass around pointers to it.
struct parseInfo {
  // ================================================================
  // Basic parameters for the target FP format

  // Number of significant bits (including hidden bit for IEEE formats)
  int sigBits;
  // Minimum/maximum binary exponents
  int minBinaryExp;
  int maxBinaryExp;
  // Total number of bytes in this format
  int bytes;
  // Approximate bounds on the decimal exponent, used for
  // early overflow/underflow checks.
  int minDecimalExp;
  int maxDecimalExp;
  // Maximum number of significant digits in the full
  // decimal representation of any exact midpoint.
  int maxDecimalMidpointDigits;

  // ================================================================
  // Inputs

  // Location where the value should be written
  unsigned char *dest;
  // First character of the provided string
  const char *start;
  // Where to put the address of the first unparsed character
  char **end;
  // Locale
  locale_t loc;

  // ================================================================
  // Outputs from the initial parse

  // First 19 digits of significand as an integer
  uint64_t digits;
  // Total number of significand digits (ignoring decimal point)
  int digitCount;
  // Address of the 20th digit or NULL if digitCount < 20
  const unsigned char *firstUnparsedDigit;
  // Number of unparsed digits = max(digitCount - 19, 0)
  int unparsedDigitCount;
  // Decimal exponent, corrected for decimal point location
  int base10Exponent;
  // True if number is negative
  int negative;
};

// ================================================================
// ================================================================
//
// Over/Underflow
//
// ================================================================
// ================================================================

static void infinity(struct parseInfo *info) {
  // 16/32/64-bit formats we can hardcode the full value
  // and memcpy() it.  This is endian-safe (assuming that
  // integers and FP values have the same endianness).
  switch (info->bytes) {
#if ENABLE_BINARY16_SUPPORT
  case 2: { // binary16
    uint16_t raw = info->negative ? 0xfc00 : 0x7c00;
    memcpy(info->dest, &raw, sizeof(raw));
    return;
  }
#endif
#if ENABLE_BINARY32_SUPPORT
  case 4: { // binary32
    uint32_t raw = info->negative ? 0xff800000UL : 0x7f800000UL;
    memcpy(info->dest, &raw, sizeof(raw));
    return;
  }
#endif
#if ENABLE_BINARY64_SUPPORT
  case 8: { // binary64
    uint64_t raw = info->negative ? 0xfff0000000000000ULL : 0x7ff0000000000000ULL;
    memcpy(info->dest, &raw, sizeof(raw));
    return;
  }
#endif
  default:
    break;
  }

  // 80- and 128-bit formats we build up incrementally.
  // TODO: Support big-endian.
  memset(info->dest, 0, info->bytes);
  switch(info->bytes) {
#if ENABLE_FLOAT80_SUPPORT
  case 10: // float80
    info->dest[7] = 0x80;
    info->dest[8] = 0xff;
    info->dest[9] = info->negative ? 0xff : 0x7f;
#endif
#if ENABLE_BINARY128_SUPPORT
  case 16: // binary128
    info->dest[14] = 0xff;
    info->dest[15] = info->negative ? 0xff : 0x7f;
    break;
#endif
  }
}

static void overflow(struct parseInfo *info) {
  // Overflow is always an ERANGE error
  errno = ERANGE;
  infinity(info);
}

// This gets invoked for inputs that are nonzero, but closer to zero
// than to the least positive or negative subnormal.  For IEEE754
// formats (and Intel x87 Extended format), these get returned as
// either +/- zero or the least subnormal, depending on the current
// rounding mode.  In all of these cases, the bit pattern is all zero
// bits except possibly for the top bit (sign) and bottom bit (either zero for
// zero or 1 for the least subnormal).
static void underflow(struct parseInfo *info) {
  // Note: C17 allows implementations to set ERANGE for underflow or not.
  // Traditionally, ERANGE has been set somewhat inconsistently:
  // gdtoa sets it for subnormals expressed in decimal format but not
  // hexadecimal; glibc uses ERANGE as an inexact flag in various cases.

  // To generally match traditional usage, this implementation sets ERANGE
  // for any subnormal return.
  // It also sets it when a non-zero input is rounded to zero.  This provides
  // a way for clients to distinguish a true zero (such as "0.0e0") from a
  // very small non-zero (such as "1e-999999").
  errno = ERANGE;
  uint8_t bottomBit = 0;
  int roundingMode = fegetround();
  if ((roundingMode == FE_DOWNWARD && info->negative)
      || (roundingMode == FE_UPWARD && !info->negative)) {
    bottomBit = 1;
  }

  // 16/32/64-bit formats we can hardcode the full value
  // and memcpy() it.  This is endian-safe (assuming that
  // integers and FP values have the same endianness).
  switch (info->bytes) {
#if ENABLE_BINARY16_SUPPORT
  case 2: { // binary16
    uint16_t raw = (info->negative ? 0x8000 : 0) | bottomBit;
    memcpy(info->dest, &raw, sizeof(raw));
    break;
  }
#endif
#if ENABLE_BINARY32_SUPPORT
  case 4: { // binary32
    uint32_t raw = (info->negative ? 0x80000000UL : 0) | bottomBit;
    memcpy(info->dest, &raw, sizeof(raw));
    break;
  }
#endif
#if ENABLE_BINARY64_SUPPORT
  case 8: { // binary64
    uint64_t raw = (info->negative ? 0x8000000000000000ULL : 0) | bottomBit;
    memcpy(info->dest, &raw, sizeof(raw));
    break;
  }
#endif
#if ENABLE_FLOAT80_SUPPORT || ENABLE_BINARY128_SUPPORT
  case 10: case 16: {
    // TODO: Make this endian-safe
    memset(info->dest, 0, info->bytes); // Initialize to +0
    info->dest[0] = bottomBit;
    info->dest[info->bytes - 1] = info->negative ? 0x80 : 0;
    break;
  }
#endif
  }
}

// ================================================================
// ================================================================
//
// General slow path
//
// ================================================================
// ================================================================


// Given a pointer to an ASCII digit string, initialize
// the mpint with the decimal value of those digits.
//
// Any non-digit characters (e.g., decimal points) are ignored.
//
// If there are more than maxDecimalMidpointDigits, then
// we stop when we've reached that many digits and then
// add exactly one digit: zero if all the remaining digits
// are zero, else one.
//
// Arguments:
//  work - pointer to the mpint
//  info - parseInfo struct
static void initMPFromDigits(mp_t *work, struct parseInfo *info) {
  // Break first19 digits into words
  mp_word_t *component = work->lsw;
  uint64_t first19 = info->digits;
  while (first19 > 0) {
    *component = (mp_word_t)first19;
    first19 >>= mp_word_bits;
    component += 1;
  }
  work->msw = component;

  // Figure out how many more digits we should parse,
  // "extra" digits are the ones beyond maxDecimalMidpointDigits.
  int remainingDigitCount = info->unparsedDigitCount;
  int extraDigitCount = 0;
  if (info->digitCount > info->maxDecimalMidpointDigits) {
    remainingDigitCount
      = info->maxDecimalMidpointDigits - (info->digitCount - info->unparsedDigitCount);
    extraDigitCount = info->unparsedDigitCount - remainingDigitCount;
  }

  // Handle remaining digits in batches of <= 9 digits
  static const uint32_t powersOfTen[] = {1, 10, 100, 1000, 10000UL,
    100000UL, 1000000UL, 10000000UL, 100000000UL, 1000000000UL};
  const unsigned char *p = info->firstUnparsedDigit;
  while (remainingDigitCount > 0) {
    int batchSize = remainingDigitCount > 9 ? 9 : remainingDigitCount;
    uint64_t batch = 0;
    for (int i = 0; i < batchSize; i++, p++) {
      unsigned t = *p - '0';
      while (t > 9) {
        p += 1; // Skip non-digits (decimal point)
        t = *p - '0';
      }
      batch = batch * 10 + t;
    }
    multiplyMPByN(work, powersOfTen[batchSize]);
    addToMP(work, batch);
    remainingDigitCount -= batchSize;
  }

  // Extra digits are summarized in a single digit:
  // zero if all the extra digits are zero, else one.
  if (extraDigitCount > 0) {
    multiplyMPByN(work, 10);
    while (extraDigitCount > 0) {
      if (*p == '0') {
        extraDigitCount -= 1;
      } else if (*p >= '1' && *p <= '9') {
        addToMP(work, 1);
        return;
      }
      // Note: Non-digit (decimal point) chars are ignored
      p += 1;
    }
  }
}

// This takes a significand and power of ten and returns the high-order
// bits of the product (correctly rounded) using exact varint arithmetic.
// This is the common slowpath used by all of the parsers below.

// General strategy: For positive exponents, the significand is
// multiplied by the power of 10 to form a large varint.  The
// high-order bits (suitably rounded) are returned as the binary
// significand.  The number of bits are returned as the
// resultExponent.  For negative exponents, the significand is divided
// by the power of ten with suitable scaling to ensure the quotient
// will have sufficient bits to return a correctly-rounded binary significand.

// This takes a pointer to a stack work area; that allows each
// format-specific implementation to pass a buffer that is
// suitably-sized.  For binary16/32/64, the passed-in buffer is sized
// so that we will never have to allocate a larger buffer on the heap,
// and those versions pass `false` for `heapAllocOK` to assert this.
// For float80/binary128, the buffer is large enough for common
// requests; larger inputs may require us to allocate a temporary
// stack buffer for the duration of this call.

// Arguments:
//  info - parseInfo struct with format details, original request
//     information, and parser output
//  roundingMode - result from fegetround()
//  stackWorkBuffer - pointer to an on-stack work area
//  stackWorkBufferBytes - size of work area in mp_word_t
//  heapAllocOK - if zero, we'll assert on any heap allocation
static void generalSlowpath(struct parseInfo *info,
                            int roundingMode,
                            mp_word_t *stackWorkArea,
                            int stackWorkAreaWords,
                            int heapAllocOK)
{
  mp_t mpSignificand;
  int binaryExponent;

  // If stackWorkArea is not big enough, space will be allocated
  // on the heap that will need to be released before we return.
  mp_word_t *heapAlloc = NULL;

  // We'll process `digitCount` digits but no more than
  // maxDecimalMidpointDigits + 1.  (See initMPWithDigits above)
  int significandDigits = info->digitCount;
  if (significandDigits > info->maxDecimalMidpointDigits) {
    significandDigits = info->maxDecimalMidpointDigits + 1;
  }
  int base10Exponent = info->base10Exponent - significandDigits + info->digitCount;

  // Figure out how many words we need for the significand and power of 5
  // 1701 / 512 is slightly bigger than log2(10) ~= 3.32
  // 1189 / 512 is slightly bigger than log2(5) ~= 2.32
  int significandBitsNeeded = (significandDigits * 1701) >> 9;
  int significandWordsNeeded = (significandBitsNeeded + (mp_word_bits - 1)) / mp_word_bits;

  int exponentBitsNeeded = (((base10Exponent < 0 ? -base10Exponent : base10Exponent) + 1) * 1189) >> 9;
  int exponentWordsNeeded = (exponentBitsNeeded + (mp_word_bits - 1)) / mp_word_bits;

  if (base10Exponent >= 0) {
    // ================================================================
    // Slow path for exponent > 0
    int totalWordsNeeded = significandWordsNeeded + exponentWordsNeeded;
    mp_t workMP;
    if (totalWordsNeeded <= stackWorkAreaWords) {
      memset(stackWorkArea, 0, stackWorkAreaWords * sizeof(mp_word_t));
      workMP.lsw = workMP.msw = stackWorkArea;
    } else {
      assert(heapAllocOK);
      heapAlloc = (mp_word_t *)calloc(totalWordsNeeded, sizeof(mp_word_t));
      if (heapAlloc == NULL) {
        memset(info->dest, 0, info->bytes);
        return;
      }
      workMP.lsw = workMP.msw = heapAlloc;
    }

    // Load workMP with digits * 5^n
    initMPFromDigits(&workMP, info);
    multiplyByFiveToTheN(&workMP, base10Exponent);
    assert((workMP.msw - workMP.lsw) <= totalWordsNeeded);

    // Bit count => binary exponent
    int bits = bitCountMP(workMP);
    binaryExponent = bits + base10Exponent; // Factor of 2^n from above
    // Leading bits => significand
    mpSignificand = shiftRightMPWithRounding(workMP,
      bits - info->sigBits, 0, info->negative, roundingMode);
    // Adjust the exponent if a round-up gave us one too many bits
    if (bitCountMP(mpSignificand) > info->sigBits) {
      binaryExponent += 1;
      mpSignificand = shiftRightMPWithTruncation(mpSignificand, 1);
    }
    if (binaryExponent > info->maxBinaryExp) {
      // Overflow.
      free(heapAlloc);
      overflow(info);
      return;
    }
  } else {
    // ================================================================
    // Slow path for exponent < 0

    // Basic idea: Since base10Exponent is negative, we can't work
    // directly with 10^base10Exponent (it's an infinite binary
    // fraction), but 10^-base10Exponent is an integer.  So we use
    // varint arithmetic to compute
    //    digits / 10^(-base10Exponent)
    // scaling the numerator so that the quotient will have at least
    // 53 bits. The tricky part is keeping the right information to do
    // accurate rounding of the result.

    // Widen numerator so the result after division will have enough bits to round
    int numeratorBitsNeeded = significandBitsNeeded;
    if (numeratorBitsNeeded < exponentBitsNeeded + info->sigBits + 2) {
      numeratorBitsNeeded = exponentBitsNeeded + info->sigBits + 2;
    }
    int numeratorWordsNeeded = (numeratorBitsNeeded + (mp_word_bits - 1)) / mp_word_bits + 2;
    int denominatorWordsNeeded = exponentWordsNeeded;
    int totalWordsNeeded = numeratorWordsNeeded + denominatorWordsNeeded;

    mp_word_t *work;
    if (totalWordsNeeded <= stackWorkAreaWords) {
      memset(stackWorkArea, 0, stackWorkAreaWords * sizeof(mp_word_t));
      work = stackWorkArea;
    } else {
      assert(heapAllocOK);
      heapAlloc = (mp_word_t *)calloc(totalWordsNeeded, sizeof(mp_word_t));
      if (heapAlloc == NULL) {
        memset(info->dest, 0, info->bytes);
        return;
      }
      work = heapAlloc;
    }

    mp_t numerator = {work, work};
    mp_t denominator = {work + numeratorWordsNeeded, work + numeratorWordsNeeded};

    // Denominator holds power of 10^N
    // (Actually 5^N, the remaining factor of 2^N is handled later.)
    fiveToTheN(&denominator, -base10Exponent);
    assert((denominator.msw - denominator.lsw) <= denominatorWordsNeeded);
    assert(denominator.msw[-1] != 0);

    // Populate numerator with digits, widen it to ensure final
    // quotient has at least sigBits + 2 bits.
    initMPFromDigits(&numerator, info);
    assert(numerator.msw[-1] != 0);
    int numeratorShift = bitCountMP(denominator) - bitCountMP(numerator) + info->sigBits + 2;
    if (numeratorShift > 0) {
      shiftLeftMP(&numerator, numeratorShift);
      assert(numerator.msw[-1] != 0);
      assert((numerator.msw - numerator.lsw) < numeratorWordsNeeded);
    } else {
      numeratorShift = 0;
    }

    // Divide, compute exact binaryExponent
    // Note: division is destructive; overwrites numerator with quotient
    int remainderNonZero;
    mp_t quotient = divideMPByMP(numerator, denominator, &remainderNonZero);
    // Binary exponent starts from number of bits in quotient
    int quotientBits = bitCountMP(quotient);
    binaryExponent = quotientBits;
    // 2^base10Exponent was omitted from the 10^N denominator above
    binaryExponent += base10Exponent;
    // We multiplied by 2^numeratorShift above, so divide by
    // 2^numeratorShift to cancel it out.
    binaryExponent -= numeratorShift;

    if (binaryExponent > info->minBinaryExp) {
      // Normal decimal
      mpSignificand = shiftRightMPWithRounding(quotient,
        quotientBits - info->sigBits, remainderNonZero, info->negative, roundingMode);
      if (bitCountMP(mpSignificand) > info->sigBits) {
        binaryExponent += 1;
        mpSignificand = shiftRightMPWithTruncation(mpSignificand, 1);
      }
      if (binaryExponent > info->maxBinaryExp) {
        free(heapAlloc);
        overflow(info);
        return;
      }
    } else if (binaryExponent > info->minBinaryExp - info->sigBits) {
      // Subnormal decimal
      int bitsNeeded = binaryExponent - (info->minBinaryExp - info->sigBits + 1);
      binaryExponent = info->minBinaryExp;
      mpSignificand = shiftRightMPWithRounding(quotient,
        quotientBits - bitsNeeded, remainderNonZero, info->negative, roundingMode);

      // Usually, overflowing the expected number of bits doesn't
      // break anything; it just results in a significand 1 bit longer
      // than we expected.

      // Except when the significand overflows into the exponent.
      // Then we have a normal, so the extra overflow bit
      // will naturally get dropped, we just have to bump the
      // exponent.
      if (bitCountMP(mpSignificand) >= info->sigBits) {
        binaryExponent += 1;
      } else {
        errno = ERANGE; // This will be a true subnormal return, set ERANGE
      }
    } else {
      // Underflow.
      free(heapAlloc);
      underflow(info);
      return;
    }
  }

  // Zero-extend to sigBits and copy to dest
  size_t mpWords = mpSignificand.msw - mpSignificand.lsw;
  size_t expectedWords = (info->sigBits + mp_word_bits - 1) / mp_word_bits;
  if (mpWords < expectedWords) {
    memset(mpSignificand.lsw + mpWords, 0, (expectedWords - mpWords) * sizeof(mp_word_t));
  }
  // TODO: Endianness.  This only works for little-endian systems.
  memcpy(info->dest, mpSignificand.lsw, (info->sigBits + 7) / 8);
  // Free the heap work area (if any)
  free(heapAlloc);

  // Set the exponent & sign bits
  uint16_t exponentBits = binaryExponent - info->minBinaryExp;
  if (info->bytes <= 8) {
    // float80 and binary128 have 16 bit exponent+sign, so no shift needed
    exponentBits <<= 16 - (info->bytes * 8 - info->sigBits + 1);
  }
  exponentBits |= info->negative ? 0x8000 : 0;

  unsigned char *p = info->dest + info->bytes;
  switch (info->bytes) {
  case 2:
    p[-1] = (info->dest[1] & 0x03) | (unsigned char)(exponentBits >> 8);
    return;
  case 4:
    p[-2] = (info->dest[2] & 0x7f) | (unsigned char)exponentBits;
    break;
  case 8:
    p[-2] = (info->dest[6] & 0x0f) | (unsigned char)exponentBits;
    break;
  case 10:
  case 16:
    p[-2] = (unsigned char)exponentBits;
    break;
  }
  p[-1] = (unsigned char)(exponentBits >> 8);
}

// ================================================================
// ================================================================
//
// Hex Float parsing
//
// ================================================================
// ================================================================

// This is called with `start` pointing to the `0x` that begins
// the hex float.  We assume the first two characters have already
// been verified to be '0x'.
static void
hexFloat(const unsigned char *start, struct parseInfo *info) {
  const unsigned char *p = start;
  p += 2; // Skip leading '0x'

  // Two 64-bit ints that we use as a joint 128-bit accumulator
  uint64_t significand_lsw = 0, significand_msw = 0;

  // Digits before the decimal point
  const unsigned char *firstDigit = p;
  unsigned remainder = 0;
  int base2Exponent = 0;
  // Perf: Just use the lower 64 bits until it's full...
  while (hexdigit[*p] < 16 && significand_lsw < (uint64_t)1 << 60) {
    significand_lsw <<= 4;
    significand_lsw += hexdigit[*p];
    p += 1;
  }
  // ... then work with the full 128 bits until it's full ...
  while (hexdigit[*p] < 16 && significand_msw < (uint64_t)1 << 59) {
    significand_msw <<= 4;
    significand_msw |= (significand_lsw >> 60);
    significand_lsw <<= 4;
    significand_lsw += hexdigit[*p];
    p += 1;
  }
  // ... if there's more beyond that, just track whether it's non-zero.
  while (hexdigit[*p] < 16) {
    remainder |= hexdigit[*p];
    base2Exponent += 4;
    p += 1;
  }
  int digitCount = (int)(p - firstDigit);

  // Try to match decimal point
  if (info->loc == NULL) {
    if (*p == '.') {
      p += 1;
    } else {
      goto possible_exponent;
    }
  } else {
    const unsigned char *decimalPoint = locale_decimal_point(info->loc);
    const unsigned char *startOfPotentialDecimalPoint = p;
    for (const unsigned char *d = decimalPoint; *d; d++) {
      if (*p != *d) {
        p = startOfPotentialDecimalPoint;
        goto possible_exponent;
      }
      p++;
    }
  }

  // Collect digits after decimal point
  const unsigned char *firstFractionDigit = p;
  if (significand_msw == 0) {
    while (hexdigit[*p] < 16 && significand_lsw < (uint64_t)1 << 60) {
      significand_lsw <<= 4;
      significand_lsw += hexdigit[*p];
      p += 1;
    }
  }
  while (hexdigit[*p] < 16 && significand_msw < (uint64_t)1 << 59) {
    significand_msw <<= 4;
    significand_msw |= (significand_lsw >> 60);
    significand_lsw <<= 4;
    significand_lsw += hexdigit[*p];
    p += 1;
  }
  // Initialize exponent from the number of digits after
  // the decimal point.
  base2Exponent -= 4 * (p - firstFractionDigit);
  // Any remaining digits may impact rounding...
  while (hexdigit[*p] < 16) {
    remainder |= hexdigit[*p];
    p += 1;
  }
  digitCount += p - firstFractionDigit;

 possible_exponent:
  if (*p == 'p' || *p == 'P') {
    const unsigned char *exponentPhrase = p;
    p += 1;
    int negativeExponent = 0;
    if (*p == '-') {
      negativeExponent = 1;
      p += 1;
    } else if (*p == '+') {
      p += 1;
    }
    if (*p < '0' || *p > '9') {
      // Ignore 'e' or 'E' not followed by number.
      p = exponentPhrase;
    } else {
      // Skip zeros in "0x1p+0000000000000000000000000001"
      int exp = 0;
      unsigned t = *p - '0';
      while (t < 10) {
        if (exp > 99999999) {
          exp = 99999999;
        } else {
          exp = exp * 10 + t;
        }
        p += 1;
        t = *p - '0';
      }
      if (negativeExponent) {
        exp = -exp;
      }
      base2Exponent += exp;
    }
  }

  if (significand_lsw == 0 && significand_msw == 0) {
    if (digitCount == 0) {
      // Malformed hexfloat with no digits after '0x',
      // BUT '0x' is still a valid zero followed by non-parsed 'x'
      p = start + 1; // Address of 'x'
    } else {
      // Just a regular zero
    }
    base2Exponent = info->minBinaryExp;
  } else {
    // Normalize to 127 bits
    if (significand_msw == 0) {
      if ((significand_lsw >> 63) == 0) {
        significand_msw = significand_lsw;
        significand_lsw = 0;
        base2Exponent -= 64;
      } else {
        significand_msw = significand_lsw >> 1;
        significand_lsw <<= 63;
        base2Exponent -= 63;
      }
    }
    int normalizeShift = __builtin_clzll(significand_msw) - 1;
    if (normalizeShift > 0) {
      significand_msw <<= normalizeShift;
      significand_msw |= significand_lsw >> (64 - normalizeShift);
      significand_lsw <<= normalizeShift;
      base2Exponent -= normalizeShift;
    }
    base2Exponent += 127;
    if (remainder) significand_lsw |= 1;

    if (base2Exponent <= info->maxBinaryExp
        && base2Exponent >= info->minBinaryExp - info->sigBits + 1) {
      int fractionBits;
      uint64_t fraction;
      if (base2Exponent > info->minBinaryExp) {
        // Hexfloat normal
        fractionBits = 127 - info->sigBits;
      } else {
        fractionBits = 127 - (base2Exponent - info->minBinaryExp + info->sigBits - 1);
        base2Exponent = info->minBinaryExp;
      }
      if (fractionBits < 64) {
        fraction = significand_lsw << (64 - fractionBits);
        significand_lsw >>= fractionBits;
        significand_lsw |= significand_msw << (64 - fractionBits);
        significand_msw >>= fractionBits;
      } else {
        fraction = (significand_msw << (128 - fractionBits))
          | (significand_lsw >> (fractionBits - 64));
        if ((significand_lsw << (128 - fractionBits)) != 0) {
          fraction |= 1;
        }
        significand_lsw = significand_msw >> (fractionBits - 64);
        significand_msw = 0;
      }

      switch (fegetround()) {
      case FE_TOWARDZERO:
        break;
      case FE_DOWNWARD:
        if (info->negative && (fraction != 0)) {
          significand_lsw += 1;
          if (significand_lsw == 0) {
            significand_msw += 1;
          }
        }
        break;
      case FE_UPWARD:
        if (!info->negative && (fraction != 0)) {
          significand_lsw += 1;
          if (significand_lsw == 0) {
            significand_msw += 1;
          }
        }
        break;
      case FE_TONEAREST:
      default: {
        const uint64_t oneHalf = (uint64_t)1 << 63;
        if (fraction > oneHalf
            || (fraction == oneHalf && (significand_lsw & 1))) {
          significand_lsw += 1;
          if (significand_lsw == 0) {
            significand_msw += 1;
          }
        }
        break;
      }
      }

      // Rounding up may have caused us to overflow:
      // For subnormals, overflow to sigBits converts this to a normal
      // For normal, overflow just needs to be renormalized
      int overflowBits = (base2Exponent == info->minBinaryExp) ? (info->sigBits - 1) : info->sigBits;
      if (((overflowBits > 64) && ((significand_msw >> (overflowBits - 64)) != 0))
          || ((overflowBits == 64) && (significand_msw != 0))
          || ((overflowBits < 64) && ((significand_msw != 0) || ((significand_lsw >> overflowBits) != 0)))) {
        if (base2Exponent > info->minBinaryExp) {
          significand_lsw >>= 1;
          significand_lsw |= significand_msw << 63;
          significand_msw >>= 1;
        }
        base2Exponent += 1;
      } else if (base2Exponent == info->minBinaryExp) {
        errno = ERANGE; // Subnormal did not overflow to normal, so set ERANGE
      }
    }
  }

  if (info->end) *info->end = (char *)p;
  if (base2Exponent > info->maxBinaryExp) {
    overflow(info);
  } else if (base2Exponent < info->minBinaryExp - info->sigBits + 1) {
    underflow(info);
  } else {
    switch (info->bytes) {
#if ENABLE_BINARY16_SUPPORT
    case 2: {
      uint16_t exponentBits = base2Exponent - info->minBinaryExp;
      uint16_t raw =
        (info->negative ? 0x8000U : 0)
        | (exponentBits << 10)
        | (significand_lsw & 0x3ffU);
      memcpy(info->dest, &raw, sizeof(raw));
      break;
    }
#endif
#if ENABLE_BINARY32_SUPPORT
    case 4: {
      uint32_t exponentBits = base2Exponent - info->minBinaryExp;
      uint32_t raw =
        (info->negative ? 0x80000000UL : 0)
        | (exponentBits << 23)
        | (significand_lsw & 0x7fffffULL);
      memcpy(info->dest, &raw, sizeof(raw));
      break;
    }
#endif
#if ENABLE_BINARY64_SUPPORT
    case 8: {
      uint64_t exponentBits = base2Exponent - info->minBinaryExp;
      uint64_t raw =
        (info->negative ? 0x8000000000000000ULL : 0)
        | (exponentBits << 52)
        | (significand_lsw & 0xfffffffffffffULL);
      memcpy(info->dest, &raw, sizeof(raw));
      break;
    }
#endif
#if ENABLE_FLOAT80_SUPPORT
    case 10: {
      // TODO: Support big-endian
      uint16_t exponentBits = base2Exponent - info->minBinaryExp;
      memcpy(info->dest, &significand_lsw, sizeof(significand_lsw));
      info->dest[8] = exponentBits & 0xff;
      info->dest[9] = (exponentBits >> 8) | (info->negative ? 0x80 : 0);
      break;
    }
#endif
#if ENABLE_BINARY128_SUPPORT
    case 16: {
      // TODO: Support big-endian
      uint16_t exponentBits = base2Exponent - info->minBinaryExp;
      memcpy(info->dest, &significand_lsw, sizeof(significand_lsw));
      memcpy(info->dest + 8, &significand_msw, sizeof(significand_msw));
      info->dest[14] = exponentBits & 0xff;
      info->dest[15] = (exponentBits >> 8) | (info->negative ? 0x80 : 0);
      break;
    }
#endif
    }
  }
}

// ================================================================
// ================================================================
//
// NaN parsing
//
// ================================================================
// ================================================================

// Parse a NaN.
// This implements the same logic as Apple's previous libc strtod.  It
// recognizes an optional payload between parentheses and uses
// that to construct a valid NaN return value.

static void parseNan(const unsigned char *start, struct parseInfo *info) {
  const unsigned char *p = start + 3; // Skip "nan"

  unsigned char stackWorkArea[20];
  memset(stackWorkArea, 0, sizeof(stackWorkArea));
  const unsigned char *endNan = p;
  if (*p == '(') {
    p += 1;
    int base = 10;
    if (*p == '0') {
      if (p[1] == 'x') {
        base = 16;
        p += 2;
      } else {
        base = 8;
        p += 1;
      }
    }
    mp_t stackMP = { (mp_word_t *)stackWorkArea,
      (mp_word_t *)(stackWorkArea + 16) };
    mp_t payload = stackMP;
    while (hexdigit[*p] < base) {
      multiplyMPByN(&payload, base);
      addToMP(&payload, hexdigit[*p]);
      payload.msw = stackMP.msw; // Prune off excess bits
      p += 1;
    }
    if (*p == ')') {
      p += 1;
    } else {
      memset(stackWorkArea, 0, sizeof(stackWorkArea));
      while (*p != '\0' && *p != ')') {
        p += 1;
      }
      if (*p == ')') {
        p += 1;
      } else {
        p = endNan;
      }
    }
  }
  // TODO: Endianness.  This only works for little-endian.
  memcpy(info->dest, stackWorkArea, info->bytes);
  switch (info->bytes) {
#if ENABLE_BINARY16_SUPPORT
  case 2: {
    info->dest[1] = info->dest[1] | (info->negative ? 0xfe : 0x7e);
    break;
  }
#endif
#if ENABLE_BINARY32_SUPPORT
  case 4: {
    info->dest[2] |= 0xc0; // Set quiet bit and low-order exponent bit
    info->dest[3] = info->negative ? 0xff : 0x7f; // exponent and sign bit
    break;
  }
#endif
#if ENABLE_BINARY64_SUPPORT
  case 8: {
    info->dest[6] |= 0xf8; // Set quiet bit and low-order exponent bits
    info->dest[7] = info->negative ? 0xff : 0x7f; // exponent and sign bit
    break;
  }
#endif
#if ENABLE_FLOAT80_SUPPORT
  case 10: {
    info->dest[7] |= 0xc0; // Set bit 63 and quiet bit
    info->dest[8] = 0xff;
    info->dest[9] = info->negative ? 0xff : 0x7f;
    break;
  }
#endif
#if ENABLE_BINARY128_SUPPORT
  case 16: {
    info->dest[13] |= 0x80; // Set quiet bit
    info->dest[14] = 0xff;
    info->dest[15] = info->negative ? 0xff : 0x7f;
    break;
  }
#endif
  }
  if (info->end) *info->end = (char *)p;
}

// This is used as the initial parse for all formats.
//
// It verifies the format and handles accordingly:
// * hexFloat is parsed by calling hexFloat() above
// * NaN payloads are parse by calling parseNan() above
// * Infinity and NaN w/o payload are parsed directly
// * True zero is parsed directly
// The above are fully handled within this function and
// it returns zero (false) to flag that there's nothing
// more to do.
//
// For a decimal input, it collects the digits into a 64-bit
// accumulator and returns that and the parsed exponent.
// If there are <= 19 digits, this is enough to generate
// a final result directly.
//
// If there are more than 19 digits, this logic first lets the
// accumulator overflow.  After we detect the overflow, we re-parse
// the first 19 digits and return those.  Callers who need more digits
// will have to use firstUnparsedDigit and digitCount to re-parse the
// significand (they can ignore the decimal point, though, since that
// has already been factored into the base10Exponent).
static int
fastParse64(struct parseInfo *info) {
  const unsigned char *p = (const unsigned char *)info->start;

  const unsigned char *firstUnparsedDigit;
  uint64_t digits = 0;
  int digitCount = 0;
  int base10Exponent = 0;
  info->negative = 0;

  // Skip leading whitespace.  Stop at a +/-/digit or other character.
  // This is a little oddly phrased in order to avoid a pointless call
  // to `isspace` for common cases.
  while (1) {
    if (*p >= '0' && *p <= '9') {
      break;
    } else if (*p == '-') {
      info->negative = 1;
      p += 1;
      break;
    } else if (*p == '+') {
      p += 1;
      break;
    } else if (*p == ' ' || isspace(*p)) {
      p += 1;
    } else {
      break;
    }
  }

  if (*p == '0') {
    if (p[1] == 'x' || p[1] == 'X') {
      hexFloat(p, info);
      return 0;
    }
    while (*p == '0') {
      p += 1;
    }
  } else if (*p == 'i' || *p == 'I') {
    if ((p[1] == 'n' || p[1] == 'N')
        && (p[2] == 'f' || p[2] == 'F')) {
      if ((p[3] == 'i' || p[3] == 'I')
          && (p[4] == 'n' || p[4] == 'N')
          && (p[5] == 'i' || p[5] == 'I')
          && (p[6] == 't' || p[6] == 'T')
          && (p[7] == 'y' || p[7] == 'Y')) {
        p += 8;
      } else {
        p += 3;
      }
      // Matched 'inf' or 'infinity' case-insensitive
      if (info->end) *info->end = (char *)p;
      infinity(info);
      return 0;
    }
    goto fail;
  } else if (*p == 'n' || *p == 'N') {
    if ((p[1] == 'a' || p[1] == 'A')
        && (p[2] == 'n' || p[2] == 'N')) {
      parseNan(p, info);
      return 0;
    }
    goto fail;
  } else if (*p < '0' || *p > '9') {
    // If this isn't a hexfloat, nan, or infinity and does
    // start with a digit, it must start with a decimal point,
    // and that decimal point must be immediately followed
    // by a digit.
    if (info->loc == NULL) {
      // Decimal point is '.' in C locale, avoid calling localeconv_l
      if (*p == '.') {
        p += 1;
        if (*p >= '0' && *p <= '9') {
          goto parseFraction;
        }
      }
    } else {
      // Look up decimal point in locale
      const unsigned char *decimalPoint = locale_decimal_point(info->loc);
      if (decimalPoint[1] == '\0') {
        if (decimalPoint[0] == *p) {
          p += 1;
          if (*p >= '0' && *p <= '9') {
            goto parseFraction;
          }
        }
      } else {
        // Multi-byte decimal point
        int matchedDecimalPoint = (1 == 1);
        for (const unsigned char *d = decimalPoint; *d; d++) {
          matchedDecimalPoint &= (*p == *d);
          p++;
        }
        if (matchedDecimalPoint && *p >= '0' && *p <= '9') {
          goto parseFraction;
        }
      }
    }
    goto fail;
  }

  // Collect digits before the decimal point
  firstUnparsedDigit = p;
  uint8_t t = *p - '0';
  if (t < 10) {
    digits = t;
    p += 1;
    t = *p - '0';
    while(t < 10) {
      digits = 10 * digits + t;
      p += 1;
      t = *p - '0';
    }
    digitCount = (int)(p - firstUnparsedDigit);
  }

  // Try to match an optional decimal point
  if (info->loc == NULL) {
    // Decimal point is '.' in C locale, avoid calling localeconv_l
    if (*p == '.') {
      p += 1;
      goto parseFraction;
    }
    goto maybeParseExponent;
  } else if (*p == ' ' || *p == '\0' || *p == 'e' || *p == 'E') {
    // Don't call localeconv_l if we know the next character
    // cannot possibly be a decimal point.
    goto maybeParseExponent;
  } else {
    // Look up decimal point in locale
    const unsigned char *decimalPoint = locale_decimal_point(info->loc);
    if (decimalPoint[1] == '\0') {
      if (decimalPoint[0] == *p) {
        p += 1;
        goto parseFraction;
      }
    } else {
      // Multi-byte decimal point
      const unsigned char *startOfPotentialDecimalPoint = p;
      int matchedDecimalPoint = (1 == 1);
      for (const unsigned char *d = decimalPoint; *d; d++) {
        matchedDecimalPoint &= (*p == *d);
        p++;
      }
      if (matchedDecimalPoint) {
        goto parseFraction;
      } else {
        p = startOfPotentialDecimalPoint;
        goto maybeParseExponent;
      }
    }
  }

 parseFraction:
  {
    const unsigned char *firstDigitAfterDecimalPoint = p;
    if (digitCount == 0) {
      while (*p == '0') {
        p += 1;
      }
      // "0.000000001234" has 4 digits
      firstUnparsedDigit = p;
      unsigned t = *p - '0';
      if (t < 10) {
        p += 1;
        digits = t;
        t = *p - '0';
        while (t < 10) {
          digits = 10 * digits + t;
          p += 1;
          t = *p - '0';
        }
      }
      digitCount = (int)(p - firstUnparsedDigit);
    } else {
      // Perf: For canada.txt benchmark, this loop is ~30% of total runtime
      unsigned t = *p - '0';
      if (t < 10) {
        p += 1;
        digits = 10 * digits + t;
        t = *p - '0';
        while (t < 10) {
          p += 1;
          digits = 10 * digits + t;
          t = *p - '0';
        }
      }
      digitCount += p - firstDigitAfterDecimalPoint;
    }
    base10Exponent = (int)(firstDigitAfterDecimalPoint - p);
  }

  // ================================================================
  // Step 1e: Parse the optional exponent
 maybeParseExponent:
  if (*p == 'e' || *p == 'E') {
    const unsigned char *exponentPhraseStart = p;
    p += 1;
    int negativeExponent = 1;
    if (*p == '-') {
      negativeExponent = -1;
      p += 1;
    } else if (*p == '+') {
      p += 1;
    }
    uint8_t t = *p - '0';
    if (t < 10) {
      int exp = t;
      p += 1;
      t = *p - '0';
      while (t < 10) {
        p += 1;
        exp = 10 * exp + t;
        t = *p - '0';
      }
      if (p - exponentPhraseStart > 9) {
        // The exponent text was unusually long... re-parse
        // it more carefully to see if it really should overflow.
        const unsigned char *q = exponentPhraseStart + 1;
        if (*q == '-' || *q == '+') {
          q += 1;
        }
        while (*q == '0') {
          q += 1;
        }
        if (p - q > 8) {
          // If there were more than 8 digits with leading zeros
          // excluded, we've definitely overflowed.
          exp = 99999999;
        }
      }
      base10Exponent += exp * negativeExponent;
    } else {
      p = exponentPhraseStart;
    }
  }

  if (info->end) *info->end = (char *)p;

  // No non-zero digits, must be an explicit zero:
  // "0", ".000", "0.0", "0e0", "0.0e999", etc.
  if (digitCount == 0) {
    memset(info->dest, 0, info->bytes);
    info->dest[info->bytes - 1] = info->negative ? 0x80 : 0;
    return 0;
  }

  // Coarse over/underflow check
  if (base10Exponent + digitCount < info->minDecimalExp) {
    underflow(info);
    return 0;
  }
  if (base10Exponent + digitCount > info->maxDecimalExp) {
    overflow(info);
    return 0;
  }

  int unparsedDigitCount = 0;
  if (digitCount > 19) {
    digits = 0;
    int i = 0;
    const unsigned char *q = firstUnparsedDigit;
    while (i < 19) {
      // Note: Skip non-digit chars (e.g., decimal point)
      unsigned t = *q - '0';
      if (t < 10) {
        digits = digits * 10 + t;
        i += 1;
      }
      q += 1;
    }
    firstUnparsedDigit = q;
    unparsedDigitCount = digitCount - 19;
  } else {
    firstUnparsedDigit = NULL;
  }

  info->digitCount = digitCount;
  info->firstUnparsedDigit = firstUnparsedDigit;
  info->unparsedDigitCount = unparsedDigitCount;
  info->digits = digits;
  info->base10Exponent = base10Exponent;

  return 1; // Regular decimal case...

 fail:
  if (info->end) *info->end = (char *)info->start;
  memset(info->dest, 0, info->bytes);
  return 0;
}

// ================================================================
// ================================================================
//
// Parse an IEEE 754 Binary16 (aka "Half")
//
// ================================================================
// ================================================================

#if ENABLE_BINARY16_SUPPORT
static void _ffpp_strtoencf16_l(unsigned char *dest, const char *start, char **end, locale_t loc) {
  static const int bytes = 2;
  static const int sigBits = 11;
  static const int minBinaryExp = -14;
  static const int maxBinaryExp = 16;
  static const int minDecimalExp = -7;
  static const int maxDecimalExp = 5;
  static const int maxDecimalMidpointDigits = 22;
  struct parseInfo info;
  info.bytes = bytes;
  info.sigBits = sigBits;
  info.minBinaryExp = minBinaryExp;
  info.maxBinaryExp = maxBinaryExp;
  info.minDecimalExp = minDecimalExp;
  info.maxDecimalExp = maxDecimalExp;
  info.maxDecimalMidpointDigits = maxDecimalMidpointDigits;
  info.dest = dest;
  info.start = start;
  info.end = end;
  info.loc = loc;

  // ================================================================
  // Parse the input (mostly)
  // ================================================================
  if (!fastParse64(&info)) {
    return;
  }

  // TODO: Someday, we might implement fast paths for binary16
  // But the range of binary16 is so small that the varint slow
  // path is actually reasonably fast.

  // ================================================================
  // Slow Path (varint calculation)
  // ================================================================
  char stackWorkArea[32];
  static const size_t stackWorkAreaWords = sizeof(stackWorkArea) / sizeof(mp_word_t);
  generalSlowpath(&info, fegetround(),  (mp_word_t *)stackWorkArea, stackWorkAreaWords, 0);
}
#endif

// ================================================================
// ================================================================
//
// Parse an IEEE 754 Binary32 (aka "Float" aka "Single")
//
// ================================================================
// ================================================================
#if ENABLE_BINARY32_SUPPORT
static void _ffpp_strtoencf32_l(unsigned char *dest, const char *start, char **end, locale_t loc) {
  static const int bytes = 4;
  static const int sigBits = 24;
  static const int minBinaryExp = -126;
  static const int maxBinaryExp = 128;
  static const int minDecimalExp = -46;
  static const int maxDecimalExp = 40;
  static const int maxDecimalMidpointDigits = 113;
  struct parseInfo info;
  info.bytes = bytes;
  info.sigBits = sigBits;
  info.minBinaryExp = minBinaryExp;
  info.maxBinaryExp = maxBinaryExp;
  info.minDecimalExp = minDecimalExp;
  info.maxDecimalExp = maxDecimalExp;
  info.maxDecimalMidpointDigits = maxDecimalMidpointDigits;
  info.dest = dest;
  info.start = start;
  info.end = end;
  info.loc = loc;

  // ================================================================
  // Parse the input (mostly)
  // ================================================================
  if (!fastParse64(&info)) {
    return;
  }

#if FLOAT_IS_BINARY32
  // ================================================================
  // Use a single float operation
  // ================================================================
  const static float floatPowerOf10[] = {1.0f, 10.0f, 100.0f,
    1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f};
  if (info.base10Exponent > -11 && info.base10Exponent < 11 && info.digitCount < 8) {
    int32_t sdigits = info.negative ? -(int32_t)info.digits : (int32_t)info.digits;
    if (info.base10Exponent < 0) {
      float result = (float)sdigits / floatPowerOf10[-info.base10Exponent];
      memcpy(info.dest, &result, sizeof(result));
      return;
    } else {
      float result = (float)sdigits * floatPowerOf10[info.base10Exponent];
      memcpy(info.dest, &result, sizeof(result));
      return;
    }
  }
#endif

  int roundingMode = fegetround();

#if 1
  // ================================================================
  // Fixed-precision interval arithmetic
  // ================================================================
  // The idea:  Use 64-bit fixed-precision arithmetic to compute
  // upper/lower bounds for the correct answer.  If those bounds
  // agree, then we can return the result.
  int16_t exp10;
  int upperBoundOffset;
  if (info.digitCount <= 19) {
    exp10 = info.base10Exponent;
    upperBoundOffset = 4;
  } else {
    exp10 = info.base10Exponent + info.digitCount - 19;
    upperBoundOffset = 36;
  }

  // Powers in the table are rounded so that
  //    powerOfTenRoundedDown <= true value <= powerOfTenRoundedDown + 1
  const uint64_t powerOfTenRoundedDown = (powersOf10_Float + 70)[exp10];

  // Binary exponent for the power-of-10 product
  const int powerOfTenExponent = binaryExponentFor10ToThe(exp10);

  // Normalize the digits, adjust binary exponent
  int normalizeDigits = __builtin_clzll(info.digits); // 0 <= normalizeDigits <= 4
  assert(normalizeDigits <= 4 || info.digitCount < 20);
  uint64_t d = info.digits << normalizeDigits;
  // For <= 19 digits, the upper bound for d is just d
  // For > 19 digits, the upper bound is 1 << normalizeDigits <= 16
  int binaryExponent = powerOfTenExponent - normalizeDigits + 64;

  // A 64-bit lower bound on the binary significand
  uint64_t l = multiply64x64RoundingDown(powerOfTenRoundedDown, d);
  // An upper bound:
  //  <= 19 digits: (powerOfTenRoundedDown + 1) * d == l128 + d
  //   > 19 digits: (powerOfTenRoundedDown + 1) * (d + 16)
  //               == l128 + d + 16 * powerOfTenRoundedDown + 16
  // For <=19 digits, upper bound is l + 2
  // For >19 digits, upper bound is l + 18

  // Normalize the product, adjust binary exponent
  // (In particular, this lets us shift by a constant below.)
  int normalizeProduct = __builtin_clzll(l); // 0 <= normalizeProduct <= 1
  assert(normalizeProduct <= 1);
  // Upper bound is <= (l + 2) << 1  or (l + 18) << 1
  l <<= normalizeProduct;
  // Upper bound is <= l + 4   or   l + 36
  binaryExponent -= normalizeProduct;

  // Upper/lower bounds for the 24-bit significand
  uint64_t u = l + upperBoundOffset;
  uint32_t lowerSignificand, upperSignificand;
  switch (roundingMode) {
  case FE_TOWARDZERO:
    lowerSignificand = (l) >> 40;
    upperSignificand = (u) >> 40;
    break;
  case FE_DOWNWARD:
    if (info.negative) {
      lowerSignificand = (l + 0x0ffffffffff) >> 40;
      upperSignificand = (u + 0x10000000000) >> 40;
    } else {
      lowerSignificand = (l) >> 40;
      upperSignificand = (l + 4) >> 40;
    }
    break;
  case FE_UPWARD:
    if (!info.negative) {
      lowerSignificand = (l + 0x0ffffffffff) >> 40;
      upperSignificand = (u + 0x10000000000) >> 40;
    } else {
      lowerSignificand = (l) >> 40;
      upperSignificand = (u) >> 40;
    }
    break;
  default:
  case FE_TONEAREST:
    // Instead of worrying about exact ties-round-even, round lower
    // down (adding 0x7ff...ff) and upper up (adding 0x800...00) so
    // that exact ties fall through to be handled elsewhere.
    lowerSignificand = (l + 0x7fffffffff) >> 40;
    upperSignificand = (u + 0x8000000000) >> 40;
  }
  if (lowerSignificand == 0) { // lowerSignificand wrapped...
    binaryExponent += 1;
  }
  if (binaryExponent > maxBinaryExp) {
    overflow(&info);
    return;
  } else if (binaryExponent <= minBinaryExp) {
    if (binaryExponent <= minBinaryExp - sigBits) {
      underflow(&info);
      return;
    }
    // TODO: ... Subnormal? ...
  } else if (upperSignificand == lowerSignificand) {
    uint32_t exponentBits = ((uint32_t)binaryExponent - minBinaryExp) << (sigBits - 1);
    uint32_t significandMask = (((uint32_t)1 << (sigBits - 1)) - 1);
    uint32_t significandBits = lowerSignificand & significandMask;
    uint32_t signbit = info.negative ? 0x80000000UL : 0ULL;
    uint32_t raw = signbit | exponentBits | significandBits;
    memcpy(dest, &raw, sizeof(raw));
    return;
  }
#endif

  // ================================================================
  // Slow Path (varint calculation)
  // ================================================================
  char stackWorkArea[128];
  static const size_t stackWorkAreaWords = sizeof(stackWorkArea) / sizeof(mp_word_t);
  generalSlowpath(&info, fegetround(),  (mp_word_t *)stackWorkArea, stackWorkAreaWords, 0);
}
#endif

// ================================================================
// ================================================================
//
// Parse an IEEE 754 Binary64 (Double)
//
// ================================================================
// ================================================================

#if ENABLE_BINARY64_SUPPORT
static void _ffpp_strtoencf64_l(unsigned char *dest, const char *start, char **end, locale_t loc) {
  static const int bytes = 8;
  static const int sigBits = 53;
  static const int minBinaryExp = -1022;
  static const int maxBinaryExp = 1024;
  static const int minDecimalExp = -325;
  static const int maxDecimalExp = 310;
  static const int maxDecimalMidpointDigits = 768;
  struct parseInfo info;
  info.bytes = bytes;
  info.sigBits = sigBits;
  info.minBinaryExp = minBinaryExp;
  info.maxBinaryExp = maxBinaryExp;
  info.minDecimalExp = minDecimalExp;
  info.maxDecimalExp = maxDecimalExp;
  info.maxDecimalMidpointDigits = maxDecimalMidpointDigits;
  info.dest = dest;
  info.start = start;
  info.end = end;
  info.loc = loc;

  // ================================================================
  // Parse the input (mostly)
  // ================================================================
  if (!fastParse64(&info)) {
    return;
  }

  // If digitCount <= 19, then the result we want is:
  //  (info.negative ? -1 : 1) * info.digits * 10^info.base10Exponent

  // The rest of this function consists of several different methods
  // of computing this product with varying trade-offs of input range
  // and performance.  The first ones are fast but only work for
  // certain inputs; the later ones are slower and more general.

  // ================================================================
  // Floating-point Calculation
  // ================================================================
  // Note: This optimization relies on the host platform `double` type
  // supporting true IEEE754 binary64 arithmetic.
#if DOUBLE_IS_BINARY64 && (FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 1)

  const static double doublePowerOf10[] = { 1.0, 10.0, 100.0, 1e3, 1e4, 1e5,
    1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16,
    1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };

  if (info.base10Exponent >= -22 && info.base10Exponent <= 18) {
    if (info.base10Exponent < 0) {
      // We can give exact inputs to a division operation if
      // we have <= 15 digits and base10Exponent is in [-22..0]
      // For example, this handles 1.23456789012345
      if (info.digitCount <= 15) {
        int64_t sdigits = info.negative ? -(int64_t)info.digits : (int64_t)info.digits;
        double result = (double)sdigits / doublePowerOf10[-info.base10Exponent];
        memcpy(info.dest, &result, sizeof(result));
        return;
      }
      // TODO: I really want to handle 35.678912345678934 here
      // (17 digits and negative exponent).  This case is the common
      // case for Lemire's "canada.txt" benchmark (which is derived from
      // latitude/longitude data).
    } else if (info.base10Exponent == 0) {
      if (info.digitCount <= 19) {
        if (!info.negative) {
          // We can use HW conversion of unsigned (positive) int
          double result = (double)info.digits;
          memcpy(info.dest, &result, sizeof(result));
          return;
        } else if (info.digits <= INT64_MAX) {
          // We can use HW conversion of signed (negative) int
          double result = (double)(-(int64_t)info.digits);
          memcpy(info.dest, &result, sizeof(result));
          return;
        }
      }
    } else {
      if (info.digitCount <= 19) {
        // We can give exact inputs to fma if we have <= 19 digits
        // and base10Exponent is in [1..18].
        int64_t lowMask = 0x7ff;
        int64_t highMask = ~lowMask;
        // digits has <= 64 bits (19 digits)
        double highDigits = (double)(info.digits & highMask); // <= 53 bits
        double lowDigits = (double)(info.digits & lowMask); // 11 bits
        double p10 = doublePowerOf10[info.base10Exponent]; // Exact (10^18 has 42 bits)
        if (info.negative) p10 = -p10;
        double u = lowDigits * p10; // Exact (11 bits + 42 bits)
        // Inputs to fma are all exact, so result is correctly rounded
        double result = fma(highDigits, p10, u);
        memcpy(info.dest, &result, sizeof(result));
        return;
      }
    }
  }
#endif

  int roundingMode = fegetround();

#if 1
  // ================================================================
  // Fixed-width interval arithmetic
  // ================================================================
  int16_t exp10;
  // Total possible inaccuracy in our calculations below
  int intervalWidth;
  if (info.digitCount <= 19) {
    exp10 = info.base10Exponent;
    // We have all the digits, so our upper/lower bounds
    // on the significand are the same, which makes the final
    // bounds fairly tight.
    intervalWidth = 12;
  } else {
    // We're going to treat the input as if it were:
    //    (first 19 digits).(remaining digits) * 10^p
    // In this form, (first 19 digits) is a lower bound for the decimal
    // significand and (first 19 digits) + 1 is an upper bound. We also
    // have upper/lower bounds for 10^p.
    // info.digits already has the first 19 digits, we just need to
    // adjust the effective exponent.
    exp10 = info.base10Exponent + info.digitCount - 19;
    // The non-trivial significand bounds lead to a wider final interval.
    // This still gives us success ~96% of the time.
    intervalWidth = 80;
  }

  // Multiply an exact value for 10^{0..27} times a rounded value 10^{n * 28}
  const int16_t coarseIndex = (exp10 * 585 + 256) >> 14; // exp10 / 28;
  const int16_t coarsePower = (coarseIndex * 32) - (coarseIndex * 4); // coarseIndex * 28
  const int16_t exactPower = exp10 - coarsePower; // exp10 % 28
  const uint64_t exact = powersOf10_Exact64[exactPower]; // Exact
  const uint64_t coarse = (powersOf10_CoarseBinary64 + 15)[coarseIndex];

  // Powers in the coarse table are never rounded up, so
  //    coarse <= true value <= coarse + 1
  const uint64_t powerOfTenRoundedDown = multiply64x64RoundingDown(coarse, exact);
  // And powerOfTenRoundedUp
  //    <= ((coarse + 1) * exact + UINT64_MAX) >> 64
  //    <= (coarse * exact + exact + UINT64_MAX) >> 64
  //    <= powerOfTenRoundedDown + 2

  // Binary exponent for the power-of-10 product
  const int powerOfTenExponent = binaryExponentFor10ToThe(coarsePower)
    + binaryExponentFor10ToThe(exactPower);

  // Normalize the digits, adjust binary exponent
  int normalizeDigits = __builtin_clzll(info.digits);
  assert(normalizeDigits <= 4 || info.digitCount < 20);
  uint64_t d = info.digits << normalizeDigits;
  int binaryExponent = powerOfTenExponent - normalizeDigits + 64;
  // The upper bound for d is:
  //   exactly d (for <= 19 digit case)
  //   d + 16 (for > 19 digit case)

  // A 64-bit lower bound on the binary significand
  uint64_t l = multiply64x64RoundingDown(powerOfTenRoundedDown, d);
  // Corresponding upper bound:
  // <= 19 digits:  (powerOfTenRoundedDown + 2) * d == l128 + d + d
  //  > 19 digits:  (powerOfTenRoundedDown + 2) * (d + 16)
  // l is a lower bound for the 64-bit binary significand
  // An upper bound for <= 19 digit case:
  //    (l128 + d + d + UINT64_MAX) >> 64   <=   l + 3
  // For >19 digits, we similarly get l + 20 as an upper bound.

  // Normalize the product, adjust binary exponent
  // (In particular, this lets us shift by a constant below.)
  int normalizeProduct = __builtin_clzll(l); // 0 <= normalizeProduct <= 2
  assert(normalizeProduct <= 2);
  // Upper bound is  (l + 3) or  (l + 20)
  l <<= normalizeProduct;
  // Upper bound is (l + 12)  or (l + 80)
  binaryExponent -= normalizeProduct;

  // Upper/lower bounds for the 53-bit significand, with rounding
  //
  // We round by adding an offset and then truncating the bottom 11
  // bits. For example, to precisely round away from zero, we
  // would ideally want to add 0x7ff.ffffffffffffffff.  Rounding that
  // value down/up to the nearest integer gives us 0x7ff, 0x800.
  int lowerRoundOffset, upperRoundOffset;
  int negative = info.negative;
  switch (roundingMode) {
  case FE_UPWARD:
    negative = !negative;
    // FALL THROUGH
  case FE_DOWNWARD:
    if (negative) {
      lowerRoundOffset = 0x7ff; upperRoundOffset = 0x800 + intervalWidth;
      break;
    }
    // FALL THROUGH
  case FE_TOWARDZERO:
    lowerRoundOffset = 0; upperRoundOffset = intervalWidth;
    break;
  default:
  case FE_TONEAREST:
    lowerRoundOffset = 0x3ff; upperRoundOffset = 0x400 + intervalWidth;
    break;
  }
  uint64_t lowerSignificand = (l + lowerRoundOffset) >> 11;
  uint64_t upperSignificand = (l + upperRoundOffset) >> 11;

  int adjustedBinaryExponent = binaryExponent;
  if (lowerSignificand == 0) { // lowerSignificand wrapped...
    adjustedBinaryExponent += 1;
  }
  if (adjustedBinaryExponent > maxBinaryExp) {
    overflow(&info);
    return;
  } else if (adjustedBinaryExponent <= minBinaryExp) {
    if (adjustedBinaryExponent <= minBinaryExp - sigBits) {
      underflow(&info);
      return;
    }
    // Might be subnormal; we need to re-round to the appropriate
    // number of bits, which requires recomputing the shift amount and
    // the rounding offsets.  This is a generalized version of the
    // code above that's also slightly slower due to the variable shifts.
    int shift = 65 - binaryExponent + (minBinaryExp - sigBits);
    if (shift < 64) {
      assert(0 < shift && shift < 64);
      uint64_t lro, uro;
      int negative = info.negative;
      switch (roundingMode) {
      case FE_UPWARD:
        negative = !negative;
        // FALL THROUGH
      case FE_DOWNWARD:
        if (negative) {
          lro = (1ULL << (shift)) - 1; uro = lro + intervalWidth; break;
        }
        // FALL THROUGH
      case FE_TOWARDZERO:
        lro = 0; uro = intervalWidth; break;
      default:
      case FE_TONEAREST:
        lro = (1ULL << (shift - 1)) - 1; uro = (1ULL << (shift - 1)) + intervalWidth; break;
      }
      lowerSignificand = (l + lro) >> shift;
      upperSignificand = (l + uro) >> shift;
      if (upperSignificand == lowerSignificand) {
        if (lowerSignificand == 0) { // lowerSignificand wrapped...
          lowerSignificand = 1ULL << (64 - shift);
        }
        // Subnormal
        uint64_t signbit = info.negative ? 0x8000000000000000ULL : 0ULL;
        uint64_t raw = signbit | lowerSignificand;
        if (raw != 0x0010000000000000ULL) {
          errno = ERANGE;
        }
        memcpy(dest, &raw, sizeof(raw));
        return;
      }
    }
  } else if (upperSignificand == lowerSignificand) {
    // Normal
    uint64_t exponentBits = ((uint64_t)adjustedBinaryExponent - minBinaryExp) << (sigBits - 1);
    uint64_t significandMask = (((uint64_t)1 << (sigBits - 1)) - 1);
    uint64_t significandBits = lowerSignificand & significandMask;
    uint64_t signbit = info.negative ? 0x8000000000000000ULL : 0ULL;
    uint64_t raw = signbit | exponentBits | significandBits;
    memcpy(dest, &raw, sizeof(raw));
    return;
  }
  // TODO: If we fall through, should we try refining the
  // estimate?  If we can extend `l` to 128 bits with very
  // little work, it might be worth the effort.
#endif

  // ================================================================
  // Slow Path (varint calculation)
  // ================================================================
  // Random testing suggests this step only runs about 3% of the
  // time, so we focus here on optimizing for code size rather than perf.
  char stackWorkArea[656];
  static const size_t stackWorkAreaWords = sizeof(stackWorkArea) / sizeof(mp_word_t);
  generalSlowpath(&info, roundingMode,  (mp_word_t *)stackWorkArea, stackWorkAreaWords, 0);
}
#endif

// ================================================================
// ================================================================
//
// 128-bit interval calculation for Float80 and Binary128
//
// Unlike binary32/64 above (where we optimize for performance),
// this logic has been generalized to support both float80 and
// binary128 with common code.  It's a little slower, but these
// formats are less commonly used, so the code savings are worth
// it.  (And we're still about 80x faster than gdtoa even with this,
// so I doubt anyone will complain about the performance here!)
//
// ================================================================
// ================================================================

#if ENABLE_FLOAT80_OPTIMIZATIONS || ENABLE_BINARY128_OPTIMIZATIONS
#if HAVE_UINT128_T
typedef __uint128_t my_uint128_t;
#define create128FromHighLow64(high,low) ((low) + ((__uint128_t)(high) << 64))
#define multiply128xInt(lhs, rhs) ((lhs) * (rhs))
#define fullMultiply64x64(lhs, rhs) ((__uint128_t)(lhs) * (rhs))
#define add128x64(lhs, rhs) ((lhs) + (rhs))
#define add128x128(lhs, rhs) ((lhs) + (rhs))
#define extractLow64(x) ((uint64_t)(x))
#define extractHigh64(x) ((uint64_t)((x) >> 64))
#define isZero(x) ((x) == 0)
#define isEqual(lhs, rhs) ((lhs) == (rhs))
#define shiftLeft(lhs, rhs) ((lhs) << (rhs))
#define shiftRight(lhs, rhs) ((lhs) >> (rhs))
#else
typedef struct { uint64_t low; uint64_t high; } my_uint128_t;
#define extractLow64(x) ((x).low)
#define extractHigh64(x) ((x).high)
#define isZero(x) ((x).low == 0 && (x).high == 0)
#define isEqual(lhs, rhs) ((lhs).low == (rhs).low && ((lhs).high == (rhs).high))
my_uint128_t shiftLeft(my_uint128_t lhs, int rhs) {
  if (rhs > 64) {
    lhs.high = lhs.low << (rhs - 64);
    lhs.low = 0;
  } else if (rhs == 64) {
    lhs.high = lhs.low;
    lhs.low = 0;
  } else if (rhs > 0) {
    lhs.high = (lhs.high << rhs) + (lhs.low >> (64 - rhs));
    lhs.low <<= rhs;
  }
  return lhs;
}
my_uint128_t shiftRight(my_uint128_t lhs, int rhs) {
  if (rhs > 64) {
    lhs.low = lhs.high >> (rhs - 64);
    lhs.high = 0;
  } else if (rhs == 64) {
    lhs.low = lhs.high;
    lhs.high = 0;
  } else if (rhs > 0) {
    lhs.low = (lhs.low >> rhs) + (lhs.high << (64 - rhs));
    lhs.high >>= rhs;
  }
  return lhs;
}
my_uint128_t create128FromHighLow64(uint64_t high, uint64_t low) {
  my_uint128_t result = {low, high};
  return result;
}
my_uint128_t add128x64(my_uint128_t lhs, uint64_t rhs) {
  if (lhs.low > UINT64_MAX - rhs) {
    lhs.high += 1;
  }
  lhs.low += rhs;
  return lhs;
}
my_uint128_t add128x128(my_uint128_t lhs, my_uint128_t rhs) {
  if (lhs.low > UINT64_MAX - rhs.low) {
    lhs.high += 1;
  }
  lhs.low += rhs.low;
  lhs.high += rhs.high;
  return lhs;
}
my_uint128_t fullMultiply64x64(uint64_t lhs, uint64_t rhs) {
  uint64_t a = (lhs >> 32) * (rhs >> 32);
  uint64_t b = (lhs >> 32) * (rhs & UINT32_MAX);
  uint64_t c = (lhs & UINT32_MAX) * (rhs >> 32);
  uint64_t d = (lhs & UINT32_MAX) * (rhs & UINT32_MAX);
  b += (c & UINT32_MAX) + (d >> 32);
  return create128FromHighLow64(a + (b >> 32) + (c >> 32),
                                (b << 32) + (d & UINT32_MAX));
}
my_uint128_t multiply128xInt(my_uint128_t lhs, int rhs) {
  uint64_t a = (lhs.low & UINT32_MAX) * rhs;
  uint64_t b = (lhs.low >> 32) * rhs;
  b += (a >> 32);
  lhs.high = (lhs.high * rhs) + (b >> 32);
  lhs.low = (a & UINT32_MAX) + (b << 32);
  return lhs;
}
#endif
static my_uint128_t multiply128x128RoundingDown(my_uint128_t lhs, my_uint128_t rhs) {
  my_uint128_t a = fullMultiply64x64(extractHigh64(lhs), extractHigh64(rhs));
  my_uint128_t b = fullMultiply64x64(extractHigh64(lhs), extractLow64(rhs));
  my_uint128_t c = fullMultiply64x64(extractLow64(lhs), extractHigh64(rhs));
  my_uint128_t d = fullMultiply64x64(extractLow64(lhs), extractLow64(rhs));
  b = add128x64(b, extractLow64(c));
  b = add128x64(b, extractHigh64(d));
  a = add128x64(a, extractHigh64(b));
  a = add128x64(a, extractHigh64(c));
  return a;
}

static my_uint128_t getPowerOfTenRoundedDown(int p, int *exponent) {
  my_uint128_t result;
  int e;

  // If power is < 0, multiply by 10^-5040 and adjust p
  // That lets us use a table with only positive powers (half the size)
  if (p < 0) {
    result = create128FromHighLow64(0xb2d31bf022977fd8ULL, 0xbf034c011f5000deULL);
    p += 5040;
    e = binaryExponentFor10ToThe(-5040);
  } else {
    result = create128FromHighLow64(1ULL << 63, 0);
    e = 1;
  }

  int finePower = p % 56;
  my_uint128_t fine;
  p -= finePower;
  if (finePower <= 27) {
    fine = create128FromHighLow64(powersOf10_Exact64[finePower], 0);
  } else if (finePower <= 54) {
    fine = fullMultiply64x64(powersOf10_Exact64[finePower - 27],
                             0xcecb8f27f4200f3aULL); // 10^27
    if ((extractHigh64(fine) >> 63) == 0) {
      fine = shiftLeft(fine, 1);
    }
  } else {
    fine = create128FromHighLow64(0xd0cf4b50cfe20765ULL, 0xfff4b4e3f741cf6dULL);
  }
  e += binaryExponentFor10ToThe(finePower);
  result = multiply128x128RoundingDown(result, fine);

  int coarseIndex = p / 56;
  const uint64_t *c = powersOf10_Binary128 + coarseIndex * 2;
  my_uint128_t coarse = create128FromHighLow64(c[1], c[0]);
  e += binaryExponentFor10ToThe(coarseIndex * 56);
  result = multiply128x128RoundingDown(result, coarse);

  *exponent = e;
  return result;
}

static int highPrecisionIntervalPath(struct parseInfo *info, int roundingMode) {
  int16_t exp10;
  int upperBoundOffset;
  if (info->digitCount <= 38) {
    exp10 = info->base10Exponent;
    upperBoundOffset = 16; // FIXME
  } else {
    exp10 = info->base10Exponent + info->digitCount - 38;
    upperBoundOffset = 272;  // FIXME
  }

  my_uint128_t digits = create128FromHighLow64(0, info->digits);
  int normalizeDigits;
  if (info->digitCount <= 19) {
    normalizeDigits = __builtin_clzll(info->digits) + 64;
  } else {
    int remaining = info->digitCount - 19;
    if (remaining > 19) { remaining = 19; }
    const unsigned char *p = info->firstUnparsedDigit;
    for (int i = 0; i < remaining; i++) {
      // Skip decimal point (depends on locale, can be any non-digit!)
      while (*p < '0' || *p > '9') {
        p++;
      }
      digits = multiply128xInt(digits, 10);
      digits = add128x64(digits, *p - '0');
      p += 1;
    }
    if (extractHigh64(digits) != 0) {
      normalizeDigits = __builtin_clzll(extractHigh64(digits));
    } else {
      normalizeDigits = __builtin_clzll(extractLow64(digits)) + 64;
    }
  }
  assert(normalizeDigits <= 5 || info->digitCount <= 38);
  digits = shiftLeft(digits, normalizeDigits);
  int binaryExponent = 128 - normalizeDigits;
  // For <= 38 digits, the upper bound for d is just d
  // For > 38 digits, the upper bound is 1 << normalizeDigits <= 32

  int powerOfTenExponent;
  const my_uint128_t powerOfTenRoundedDown = getPowerOfTenRoundedDown(exp10, &powerOfTenExponent);
  binaryExponent += powerOfTenExponent;
  //    powerOfTenRoundedDown <= true value <= powerOfTenRoundedDown + 2

  // A 128-bit lower bound on the binary significand
  my_uint128_t l = multiply128x128RoundingDown(powerOfTenRoundedDown, digits);

  // (In particular, this lets us shift by a constant below.)
  int normalizeProduct = __builtin_clzll(extractHigh64(l)); // 0 <= normalizeProduct <= 1
  // Upper bound is <= (l + 2) or (l + 34)
  assert(normalizeProduct <= 3);
  l = shiftLeft(l, normalizeProduct);
  // Upper bound is <= l + 16   or   l + 272
  binaryExponent -= normalizeProduct;

  // Upper/lower bounds for the 64-bit significand
  my_uint128_t u = add128x64(l, upperBoundOffset);
  int negative = info->negative;
  switch (roundingMode) {
  case FE_DOWNWARD:
    negative = !negative;
    // FALL THROUGH
  case FE_UPWARD:
    if (!negative) {
      my_uint128_t offset = create128FromHighLow64(UINT64_MAX, UINT64_MAX);
      offset = shiftRight(offset, info->sigBits);
      l = add128x128(l, offset);
      u = add128x128(u, offset);
      u = add128x64(u, 1);
      break;
    }
    // FALL THROUGH
  case FE_TOWARDZERO:
    break;
  default:
  case FE_TONEAREST: {
    my_uint128_t offset = create128FromHighLow64(UINT64_MAX, UINT64_MAX);
    offset = shiftRight(offset, info->sigBits + 1);
    l = add128x128(l, offset);
    u = add128x128(u, offset);
    u = add128x64(u, 1);
  }
  }
  my_uint128_t lowerSignificand = shiftRight(l, (128 - info->sigBits));
  my_uint128_t upperSignificand = shiftRight(u, (128 - info->sigBits));
  if (isZero(lowerSignificand)) { // lowerSignificand wrapped...
    binaryExponent += 1;
    lowerSignificand = create128FromHighLow64(1ULL << 63, 0);
    lowerSignificand = shiftRight(lowerSignificand, 128 - info->sigBits);
  }
  if (binaryExponent > info->maxBinaryExp) {
    overflow(info);
    return 1;
  } else if (binaryExponent <= info->minBinaryExp) {
    if (binaryExponent <= info->minBinaryExp - info->sigBits) {
      underflow(info);
      return 1;
    }
    // TODO: ... Subnormal? ...
  } else if (isEqual(upperSignificand, lowerSignificand)) {
    switch (info->bytes) {
    case 10: {
      uint16_t signbit = info->negative ? 0x8000U : 0U;
      uint16_t exponentBits = signbit | ((uint16_t)binaryExponent - info->minBinaryExp);
      uint64_t significandBits = extractLow64(lowerSignificand);
      memcpy(info->dest, &significandBits, sizeof(significandBits));
      memcpy(info->dest + 8, &exponentBits, sizeof(exponentBits));
      return 1;
    }
    case 16: {
      uint16_t signbit = info->negative ? 0x8000U : 0U;
      uint16_t exponentBits = signbit | ((uint16_t)binaryExponent - info->minBinaryExp);
      memcpy(info->dest, &lowerSignificand, sizeof(lowerSignificand));
      memcpy(info->dest + 14, &exponentBits, sizeof(exponentBits));
      return 1;
    }
    }
  }
  return 0;
}
#endif

// ================================================================
// ================================================================
//
// Parse an Intel x87 80-bit extended format value
//
// ================================================================
// ================================================================

#if ENABLE_FLOAT80_SUPPORT
static void _ffpp_strtoencf80_l(unsigned char *dest, const char *start, char **end, locale_t loc) {
  static const int bytes = 10;
  static const int sigBits = 64;
  static const int minBinaryExp = -16382;
  static const int maxBinaryExp = 16384;
  static const int minDecimalExp = -5000;
  static const int maxDecimalExp = 5000;
  static const int maxDecimalMidpointDigits = 11515;
  struct parseInfo info;
  info.bytes = bytes;
  info.sigBits = sigBits;
  info.minBinaryExp = minBinaryExp;
  info.maxBinaryExp = maxBinaryExp;
  info.minDecimalExp = minDecimalExp;
  info.maxDecimalExp = maxDecimalExp;
  info.maxDecimalMidpointDigits = maxDecimalMidpointDigits;
  info.dest = dest;
  info.start = start;
  info.end = end;
  info.loc = loc;

  // ================================================================
  // Parse the input (mostly)
  // ================================================================
  if (!fastParse64(&info)) {
    return;
  }

#if ENABLE_FLOAT80_OPTIMIZATIONS && LONG_DOUBLE_IS_FLOAT80
  // ================================================================
  // Use a single float80 operation when we can
  // ================================================================
  const static long double longDoublePowerOf10[] = {1.0L, 10.0L, 100.0L,
    1e3L, 1e4L, 1e5L, 1e6L, 1e7L, 1e8L, 1e9L, 1e10L, 1e11L, 1e12L,
    1e13L, 1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L, 1e20L, 1e21L,
    1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L};
  if (info.base10Exponent > -28 && info.base10Exponent < 28 && info.digitCount <= 19) {
    if (info.base10Exponent < 0) {
      long double p = longDoublePowerOf10[-info.base10Exponent];
      if (info.negative) p = -p;
      long double result = (long double)info.digits / p;
      memcpy(info.dest, &result, sizeof(result));
      return;
    } else {
      long double p = longDoublePowerOf10[info.base10Exponent];
      if (info.negative) p = -p;
      long double result = (long double)info.digits * p;
      memcpy(info.dest, &result, sizeof(result));
      return;
    }
  }
#endif

  int roundingMode = fegetround();

#if ENABLE_FLOAT80_OPTIMIZATIONS
  if (highPrecisionIntervalPath(&info, roundingMode)) {
    return;
  }
#endif

  // ================================================================
  // Slow Path (varint calculation)
  // ================================================================
  char stackWorkArea[1536];
  static const size_t stackWorkAreaWords = sizeof(stackWorkArea) / sizeof(mp_word_t);
  generalSlowpath(&info, roundingMode,  (mp_word_t *)stackWorkArea, stackWorkAreaWords, 1);
}
#endif

// ================================================================
// ================================================================
//
// Parse an IEEE 754 Binary128
//
// ================================================================
// ================================================================

#if ENABLE_BINARY128_SUPPORT
static void _ffpp_strtoencf128_l(unsigned char *dest, const char *start, char **end, locale_t loc) {
  static const int bytes = 16;
  static const int sigBits = 113;
  static const int minBinaryExp = -16382;
  static const int maxBinaryExp = 16384;
  static const int minDecimalExp = -5000;
  static const int maxDecimalExp = 5000;
  static const int maxDecimalMidpointDigits = 11564;
  struct parseInfo info;
  info.bytes = bytes;
  info.sigBits = sigBits;
  info.minBinaryExp = minBinaryExp;
  info.maxBinaryExp = maxBinaryExp;
  info.minDecimalExp = minDecimalExp;
  info.maxDecimalExp = maxDecimalExp;
  info.maxDecimalMidpointDigits = maxDecimalMidpointDigits;
  info.dest = dest;
  info.start = start;
  info.end = end;
  info.loc = loc;

  // ================================================================
  // Parse the input (mostly)
  // ================================================================
  if (!fastParse64(&info)) {
    return;
  }

#if LONG_DOUBLE_IS_BINARY128 && ENABLE_BINARY128_OPTIMIZATIONS
  // ================================================================
  // Use a single binary128 operation when we can
  // ================================================================
  const static long double longDoublePowerOf10[] = {
    1.0L, 10.0L, 100.0L, 1e3L, 1e4L, 1e5L, 1e6L, 1e7L, 1e8L, 1e9L,
    1e10L, 1e11L, 1e12L, 1e13L, 1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L,
    1e20L, 1e21L, 1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L, 1e28L, 1e29L,
    1e30L, 1e31L, 1e32L, 1e33L, 1e34L, 1e35L, 1e36L, 1e37L, 1e38L, 1e39L,
    1e40L, 1e41L, 1e42L, 1e43L, 1e44L, 1e45L, 1e46L, 1e47L, 1e48L};
  if (info.base10Exponent > -49 && info.base10Exponent < 49 && info.digitCount <= 19) {
    if (info.base10Exponent < 0) {
      long double p = longDoublePowerOf10[-info.base10Exponent];
      if (info.negative) p = -p;
      long double result = (long double)info.digits / p;
      memcpy(info.dest, &result, sizeof(result));
      return;
    } else {
      long double p = longDoublePowerOf10[info.base10Exponent];
      if (info.negative) p = -p;
      long double result = (long double)info.digits * p;
      memcpy(info.dest, &result, sizeof(result));
      return;
    }
  }
#endif

  int roundingMode = fegetround();

#if ENABLE_BINARY128_OPTIMIZATIONS
  if (highPrecisionIntervalPath(&info, roundingMode)) {
    return;
  }
#endif

  // ================================================================
  // Slow Path (varint calculation)
  // ================================================================
  char stackWorkArea[1536];
  static const size_t stackWorkAreaWords = sizeof(stackWorkArea) / sizeof(mp_word_t);
  generalSlowpath(&info, roundingMode,  (mp_word_t *)stackWorkArea, stackWorkAreaWords, 1);
}
#endif

// ================================================================
// ================================================================
//
// Public APIs
//
// The public functions exported from this file are all defined
// in terms of the private `_ffpp_strtoencf**_l` functions defined
// above.
//
// ================================================================
// ================================================================

// ================================================================
// Wrappers for Binary16
#if ENABLE_BINARY16_SUPPORT
// TS 18661-3 `strtoencf16` API that can be supported on
// every platform regardless of local FP
void strtoencf16(unsigned char * restrict encptr,
                      const char * restrict nptr,
                      char ** restrict endptr) {
 _ffpp_strtoencf16_l(encptr, nptr, endptr, LC_GLOBAL_LOCALE);
}
#endif

// ================================================================
// Wrappers for Binary32

#if ENABLE_BINARY32_SUPPORT
// TS 18661-3 `strtoencf32` API that can be supported on
// every platform regardless of local FP
void strtoencf32(unsigned char * restrict encptr,
                      const char * restrict nptr,
                      char ** restrict endptr) {
  _ffpp_strtoencf32_l(encptr, nptr, endptr, LC_GLOBAL_LOCALE);
}
#endif

#if ENABLE_BINARY32_SUPPORT && FLOAT_IS_BINARY32
// ISO C17 `strtof` API
float strtof(const char * restrict nptr,
                   char ** restrict endptr) {
  union { float d; unsigned char raw[4]; } result;
  _ffpp_strtoencf32_l(result.raw, nptr, endptr, LC_GLOBAL_LOCALE);
  return result.d;
}
#endif

#if ENABLE_BINARY32_SUPPORT && FLOAT_IS_BINARY32
// ISO C17 `strtof_l` API
float strtof_l(const char * restrict nptr,
                     char ** restrict endptr,
                     locale_t loc) {
  union { float d; unsigned char raw[4]; } result;
  _ffpp_strtoencf32_l(result.raw, nptr, endptr, loc);
  return result.d;
}
#endif

// ================================================================
// Wrappers for Binary64

#if ENABLE_BINARY64_SUPPORT
// TS 18661-3 `strtoencf64` API that can be supported on
// every platform regardless of local FP
void strtoencf64(unsigned char * restrict encptr,
                      const char * restrict nptr,
                      char ** restrict endptr) {
  _ffpp_strtoencf64_l(encptr, nptr, endptr, LC_GLOBAL_LOCALE);
}
#endif

#if ENABLE_BINARY64_SUPPORT && LONG_DOUBLE_IS_BINARY64
// TS 18661-3 `strtoencf64x` API
// If `long double` is binary64, we assume Float64x is binary64
void strtoencf64x(unsigned char *restrict encptr,
                       const char * restrict nptr,
                       char ** restrict endptr) {
  _ffpp_strtoencf64_l(encptr, nptr, endptr, LC_GLOBAL_LOCALE);
}
#endif

#if ENABLE_BINARY64_SUPPORT && DOUBLE_IS_BINARY64
// ISO C17 `strtod` API
double strtod(const char * restrict nptr,
                   char ** restrict endptr) {
  union { double d; unsigned char raw[8]; } result;
  _ffpp_strtoencf64_l(result.raw, nptr, endptr, LC_GLOBAL_LOCALE);
  return result.d;
}
#endif

#if ENABLE_BINARY64_SUPPORT && DOUBLE_IS_BINARY64
// ISO C17 `strtod_l` API
double strtod_l(const char * restrict nptr,
                     char ** restrict endptr,
                     locale_t loc) {
  union { double d; unsigned char raw[8]; } result;
  _ffpp_strtoencf64_l(result.raw, nptr, endptr, loc);
  return result.d;
}
#endif

#if ENABLE_BINARY64_SUPPORT && LONG_DOUBLE_IS_BINARY64
// ISO C17 `strtold` API
long double strtold(const char * restrict nptr,
                         char ** restrict endptr) {
  union { long double d; unsigned char raw[8]; } result;
  _ffpp_strtoencf64_l(result.raw, nptr, endptr, LC_GLOBAL_LOCALE);
  return result.d;
}
#endif

#if ENABLE_BINARY64_SUPPORT && LONG_DOUBLE_IS_BINARY64
// ISO C17 `strtold_l` API
long double strtold_l(const char * restrict nptr,
                           char ** restrict endptr,
                           locale_t loc) {
  union { long double d; unsigned char raw[8]; } result;
  _ffpp_strtoencf64_l(result.raw, nptr, endptr, loc);
  return result.d;
}
#endif

// ================================================================
// Wrappers for Float80

#if ENABLE_FLOAT80_SUPPORT
// Non-standard but helpful for testing.
void strtoencf80_l(unsigned char *restrict encptr,
                        const char * restrict nptr,
                        char ** restrict endptr,
                        locale_t loc) {
  _ffpp_strtoencf80_l(encptr, nptr, endptr, loc);
}
#endif

#if ENABLE_FLOAT80_SUPPORT && LONG_DOUBLE_IS_FLOAT80
// TS 18661-3 `strtoencf64x` API
// If `long double` is float80, assume `Float64x` is float80
void strtoencf64x(unsigned char *restrict encptr,
                       const char * restrict nptr,
                       char ** restrict endptr) {
  _ffpp_strtoencf80_l(encptr, nptr, endptr, LC_GLOBAL_LOCALE);
}
#endif

#if ENABLE_FLOAT80_SUPPORT && LONG_DOUBLE_IS_FLOAT80
// ISO C17 `strtold` API
long double strtold(const char * restrict nptr,
                         char ** restrict endptr) {
  union { long double d; unsigned char raw[10]; } result;
  _ffpp_strtoencf80_l(result.raw, nptr, endptr, LC_GLOBAL_LOCALE);
  return result.d;
}
#endif

#if ENABLE_FLOAT80_SUPPORT && LONG_DOUBLE_IS_FLOAT80
// ISO C17 `strtold` API
long double strtold_l(const char * restrict nptr,
                           char ** restrict endptr,
                           locale_t loc) {
  union { long double d; unsigned char raw[10]; } result;
  _ffpp_strtoencf80_l(result.raw, nptr, endptr, loc);
  return result.d;
}
#endif

// ================================================================
// Wrappers for Binary128

#if ENABLE_BINARY128_SUPPORT
// TS 18661-3 `strtoencf128` API that can be supported on
// every platform regardless of local FP
void strtoencf128(unsigned char * restrict encptr,
                       const char * restrict nptr,
                       char ** restrict endptr) {
  _ffpp_strtoencf128_l(encptr, nptr, endptr, LC_GLOBAL_LOCALE);
}
#endif

#if ENABLE_BINARY128_SUPPORT && LONG_DOUBLE_IS_BINARY128
// ISO C17 `strtold` API
long double strtold(const char * restrict nptr,
                         char ** restrict endptr) {
  union { long double d; unsigned char raw[16]; } result;
  _ffpp_strtoencf128_l(result.raw, nptr, endptr, LC_GLOBAL_LOCALE);
  return result.d;
}
#endif

#if ENABLE_BINARY128_SUPPORT && LONG_DOUBLE_IS_BINARY128
// ISO C17 `strtold_l` API
long double strtold_l(const char * restrict nptr,
                           char ** restrict endptr,
                           locale_t loc) {
  union { long double d; unsigned char raw[16]; } result;
  _ffpp_strtoencf128_l(result.raw, nptr, endptr, loc);
  return result.d;
}
#endif
