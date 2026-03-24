/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#ifndef __MVM_TESTING_H__
#define __MVM_TESTING_H__

#include "../src/platform.h"

#include <darwintest.h>

#if !MALLOC_TARGET_EXCLAVES
#include <mach/mach.h> // mach_task_self
#include <os/tsd.h> // _os_cpu_number
#endif // !MALLOC_TARGET_EXCLAVES

#if !MALLOC_TARGET_EXCLAVES || defined(MVM_INCLUDE_SOURCE)

// Provide one definition of these symbols
#include "../src/vm.c"

bool
malloc_tracing_enabled = false;

malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;

static void __printflike(1,0)
print_output(const char *fmt, va_list va)
{
	char buf[256];

	// Use a fixed-size buffer here, because libdarwintest will internally call
	// vasprintf, which will allocate and may end up recursing back inside
	// libmalloc if we are the one trying to print
	vsnprintf(buf, sizeof(buf), fmt, va);
	T_LOG("%s", buf);
}

void
malloc_zone_error(uint32_t flags, bool is_corruption, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	print_output(fmt, va);
	va_end(va);

	__builtin_trap();
}

void
malloc_report(uint32_t flags, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	print_output(fmt, va);
	va_end(va);
}

#endif // !MALLOC_TARGET_EXCLAVES || defined(MVM_INCLUDE_SOURCE)

#endif // __MVM_TESTING_H__
