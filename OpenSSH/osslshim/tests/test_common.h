/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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

#ifndef _TEST_COMMON_H_
#define _TEST_COMMON_H_

#include <stdint.h>

/*
 *  General Data Structures and Macros for test functions/vectors.
 */

/* Test function type */
typedef const struct {
	int (*funcptr)(void);
	const char *description;
} testFunction_t;

/* String vector type */
typedef const struct {
	const size_t	len;
	const char *	value;
} stringVector_t;

/* Byte vector type */
typedef const struct {
	const size_t	len;
	const uint8_t	value[]; /* flexible array member (needs to be last) */
} byteVector_t;

/* Array initialization macros. (Limited to 25 test vectors per test) */
#define ARRAY_FOR_1(N)		[0] = N ## 0,
#define ARRAY_FOR_2(N)		ARRAY_FOR_1(N) [1] = N ## 1,
#define ARRAY_FOR_3(N)		ARRAY_FOR_2(N) [2] = N ## 2,
#define ARRAY_FOR_4(N)		ARRAY_FOR_3(N) [3] = N ## 3,
#define ARRAY_FOR_5(N)		ARRAY_FOR_4(N) [4] = N ## 4,
#define ARRAY_FOR_6(N)		ARRAY_FOR_5(N) [5] = N ## 5,
#define ARRAY_FOR_7(N)		ARRAY_FOR_6(N) [6] = N ## 6,
#define ARRAY_FOR_8(N)		ARRAY_FOR_7(N) [7] = N ## 7,
#define ARRAY_FOR_9(N)		ARRAY_FOR_8(N) [8] = N ## 8,
#define ARRAY_FOR_10(N)		ARRAY_FOR_9(N) [9] = N ## 9,
#define ARRAY_FOR_11(N)		ARRAY_FOR_10(N) [10] = N ## 10,
#define ARRAY_FOR_12(N)		ARRAY_FOR_11(N) [11] = N ## 11,
#define ARRAY_FOR_13(N)		ARRAY_FOR_12(N) [12] = N ## 12,
#define ARRAY_FOR_14(N)		ARRAY_FOR_13(N) [13] = N ## 13,
#define ARRAY_FOR_15(N)		ARRAY_FOR_14(N) [14] = N ## 14,
#define ARRAY_FOR_16(N)		ARRAY_FOR_15(N) [15] = N ## 16,
#define ARRAY_FOR_17(N)		ARRAY_FOR_16(N) [16] = N ## 16,
#define ARRAY_FOR_18(N)		ARRAY_FOR_17(N) [17] = N ## 17,
#define ARRAY_FOR_19(N)		ARRAY_FOR_18(N) [18] = N ## 18,
#define ARRAY_FOR_20(N)		ARRAY_FOR_19(N) [19] = N ## 19,
#define ARRAY_FOR_21(N)		ARRAY_FOR_20(N) [20] = N ## 20,
#define ARRAY_FOR_22(N)		ARRAY_FOR_21(N) [21] = N ## 21,
#define ARRAY_FOR_23(N)		ARRAY_FOR_22(N) [22] = N ## 22,
#define ARRAY_FOR_24(N)		ARRAY_FOR_23(N) [23] = N ## 23,
#define ARRAY_FOR_25(N)		ARRAY_FOR_24(N) [24] = N ## 24,

#endif /* _TEST_COMMON_H_ */
