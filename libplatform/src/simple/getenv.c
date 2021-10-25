/*
 * Copyright (c) 2013 Apple Computer, Inc. All rights reserved.
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

#include <TargetConditionals.h>

#include <stdlib.h>

#include <_simple.h>

#include <platform/string.h>
#include <platform/compat.h>

// This file is built with -fno-builtin to prevent the compiler from turning
// _simple_memcmp into a memcmp() call.

int
_simple_memcmp(const void *s1, const void *s2, size_t n)
{
	if (n != 0) {
		const unsigned char *p1 = s1, *p2 = s2;

		do {
			if (*p1++ != *p2++)
				return (*--p1 - *--p2);
		} while (--n != 0);
	}
	return (0);
}

const char *
_simple_getenv(const char *envp[], const char *var) {
    const char **p;
    size_t var_len;

    var_len = strlen(var);

    for (p = envp; p && *p; p++) {
        size_t p_len = strlen(*p);

        if (p_len >= var_len &&
            _simple_memcmp(*p, var, var_len) == 0 &&
            (*p)[var_len] == '=') {
            return &(*p)[var_len + 1];
        }
    }

    return NULL;
}
