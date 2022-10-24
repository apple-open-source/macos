/*
 * Copyright (c) 2021 Apple, Inc. All rights reserved.
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
 *
 * This file implements the following function generically:
 *
 *  unsigned long memcmp_zero_aligned8(const void *s, size_t n);
 *
 * memcmp_zero_aligned8() checks whether string s of n bytes contains all
 * zeros.  Address and size of the string s must be 8 byte-aligned.  Returns 0
 * if true, 1 otherwise. Also returns 0 if n is 0.
 */

#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMCMP_ZERO_ALIGNED8

#if defined(__LP64__)

unsigned long
_platform_memcmp_zero_aligned8(const void *s, size_t n)
{
	size_t size = n;

	if (size == 0) {
		return 0;
	}

	const uint64_t *p = (const uint64_t *)s;
	uint64_t a = p[0];

	_Static_assert(sizeof(unsigned long) == sizeof(uint64_t), "");

	if (size < 4 * sizeof(uint64_t)) {
		if (size > 1 * sizeof(uint64_t)) {
			a |= p[1];
			if (size > 2 * sizeof(uint64_t)) {
				a |= p[2];
			}
		}
	} else {
		size_t count = size / sizeof(uint64_t);
		uint64_t b = p[1];
		uint64_t c = p[2];
		uint64_t d = p[3];

		/*
		 * note: for sizes not a multiple of 32 bytes, this will load
		 * the bytes [size % 32 .. 32) twice which is ok
		 */
		while (count > 4) {
			count -= 4;
			a |= p[count + 0];
			b |= p[count + 1];
			c |= p[count + 2];
			d |= p[count + 3];
		}

		a |= b | c | d;
	}

	return (a != 0);
}

#else // defined(__LP64__)

unsigned long
_platform_memcmp_zero_aligned8(const void *s, size_t n)
{
	uintptr_t p = (uintptr_t)s;
	uintptr_t end = (uintptr_t)s + n;
	uint32_t a, b;

	_Static_assert(sizeof(unsigned long) == sizeof(uint32_t), "");

	a = 0;
	b = 0;

	for (; p < end; p += sizeof(uint64_t)) {
		uint64_t v = *(const uint64_t *)p;
		a |= (uint32_t)v;
		b |= (uint32_t)(v >> 32);
	}

	return (a | b) != 0;
}

#endif // defined(__LP64__)

#endif // !_PLATFORM_OPTIMIZED_MEMCMP_ZERO_ALIGNED8
