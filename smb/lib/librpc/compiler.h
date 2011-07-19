/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#ifndef COMPILER_H_09EABBF7_1566_4BAB_B665_CBD0D0831CFB
#define COMPILER_H_09EABBF7_1566_4BAB_B665_CBD0D0831CFB

#include "platform.h"

#include <stdint.h> //
#if PLATFORM(UNIX)
#include <sys/types.h> // ssize_t
#endif
	
	

/*!
 * @define UNLIKELY
 * Marks an expression as likely to evaluate to FALSE.
 */
#ifndef UNLIKELY
#if COMPILER(GCC)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UNLIKELY(x) (x)
#endif
#endif

/*!
 * @define LIKELY
 * Marks an expression as likely to evaluate to TRUE.
 */
#ifndef LIKELY
#if COMPILER(GCC)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define LIKELY(x) (x)
#endif
#endif

/*!
 * @define REFERENCE_SYMBOL
 * Mark a symbol as referenced so that the static linker won't strip it.
 */
#if PLATFORM(DARWIN)
/* Force strip(1) to preserve SYM by setting the REFERENCED_DYNAMICALLY bit. */
#define REFERENCE_SYMBOL(sym) asm(".desc " #sym ", 0x10");
#else
#define REFERENCE_SYMBOL(sym) (sym)
#endif

/*!
 * @define PRINTFLIKE
 * @abstract Mark a function as a candidate for printf(3) format string checking.
 * @param fmtarg The number of the format string argument.
 * @param firstvararg The number of the first argument used in the format string.
 */
#ifndef PRINTFLIKE
#if COMPILER(GCC)
#define PRINTFLIKE(fmtarg, firstvararg) \
    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define PRINTFLIKE(fmtarg, firstvararg)
#endif /* COMPILER(GCC) */
#endif

#ifdef __cplusplus
/*!
 * @define CXX_NONCOPYABLE
 * @abstract Disable copy construction and assignent for this class.
 */
#define CXX_NONCOPYABLE(CLASSNAME) \
    CLASSNAME(const CLASSNAME&); const CLASSNAME& operator=(const CLASSNAME&)
#endif

#if COMPILER(GCC)
// Return the required alignment of the given type.
#define alignof(x) ((uintptr_t)__alignof__(x))
#define __hidden __attribute__((visibility("hidden")))

// XXX We might have to do some funkier preprocessor checks when we get the
// C++0x alignof keyword.
#else
#define alignof(t) ((uintptr_t)offsetof(struct { char c; t x; }, x))
#define __hidden
#endif

// Define explicit UTF project types, and convenience casts
#if !defined(_utf32_t_DEFINED)
#define  _utf32_t_DEFINED
typedef uint32_t utf32_t;
#endif

#if !defined(_utf16_t_DEFINED)
#define  _utf16_t_DEFINED
typedef uint16_t utf16_t;
#endif

#if !defined(_utf8_t_DEFINED)
#define _utf8_t_DEFINED
typedef uint8_t utf8_t;
#endif

// Cosmetic conversion fro utf8_t * to char *.
static inline
char * utf8_cast(utf8_t * c) {
    return reinterpret_cast<char *>(c);
}

// Cosmetic conversion from const utf8_t * to const char *.
static inline
const char * utf8_cast(const utf8_t * c) {
    return reinterpret_cast<const char *>(c);
}

// Return the size of an array literal.
template <typename T, unsigned N>
unsigned array_size(const T (&)[N]) {
    return N;
}

// Return an iterator to the end of an array literal.
template <typename T, unsigned N>
T * array_end(T (&a)[N]) {
    return a + N;
}

// Return an iterator to the end of a const array literal.
template <typename T, unsigned N>
const T * array_end(const T (&a)[N]) {
    return a + N;
}

// Copy an array literal.
template <typename T, unsigned N>
void array_copy(T (&dst)[N], const T (&src)[N]) {
    memcpy(dst, src, N * sizeof(T));
}

#endif /* COMPILER_H_09EABBF7_1566_4BAB_B665_CBD0D0831CFB */

/* vim: set cindent et ts=4 sw=4 tw=79 : */
