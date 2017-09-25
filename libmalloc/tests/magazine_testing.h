/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#ifndef __MAGAZINE_TESTING
#define __MAGAZINE_TESTING

// Import the mvm_* functions for magazines to use
#import "../src/vm.c"

#import "../src/magazine_rack.c"

boolean_t
malloc_tracing_enabled = 0;

// Stub out cross-file depedencies so that they just assert.
void _malloc_printf(int flags, const char *fmt, ...)
{
	__builtin_trap();
}

void
szone_free(szone_t *szone, void *ptr)
{
	__builtin_trap();
}

void *
szone_malloc(szone_t *szone, size_t size)
{
	__builtin_trap();
}

#endif // __MAGAZINE_TESTING
