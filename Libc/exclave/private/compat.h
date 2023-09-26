/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef _PRIVATE_COMPAT_H_
#define _PRIVATE_COMPAT_H_

#include <sys/types.h>

/* Build sources in simple mode */
#define BUILDING_SIMPLE 1

/* Missing definition in sys/cdefs.h */
#define __FBSDID(s)

/* For bsearch_b */
__attribute__((visibility("hidden")))
void	*bsearch(const void *__key, const void *__base, size_t __nel,
		size_t __width, int (* _Nonnull __compar)(const void *, const void *));

/* For qsort_b */
#define flsl(x) ((sizeof(x) << 3) - __builtin_clzl(x))

__attribute__((visibility("hidden")))
void	 qsort_r(void *__base, size_t __nel, size_t __width, void *,
		int (* _Nonnull __compar)(void *, const void *, const void *));

/* For merge_b */
typedef unsigned char u_char;

/* For strtofp.c */
#define ENABLE_LOCALE_SUPPORT 0

__attribute__((visibility("hidden")))
void strtoencf16(unsigned char * restrict encptr,
                      const char * restrict nptr,
                      char ** restrict endptr);

__attribute__((visibility("hidden")))
void strtoencf32(unsigned char * restrict encptr,
                      const char * restrict nptr,
                      char ** restrict endptr);

__attribute__((visibility("hidden")))
void strtoencf64(unsigned char * restrict encptr,
                      const char * restrict nptr,
                      char ** restrict endptr);

__attribute__((visibility("hidden")))
void strtoencf64x(unsigned char *restrict encptr,
                       const char * restrict nptr,
                       char ** restrict endptr);

__attribute__((visibility("hidden")))
void strtoencf128(unsigned char * restrict encptr,
                       const char * restrict nptr,
                       char ** restrict endptr);

#endif /* _PRIVATE_COMPAT_H_ */
