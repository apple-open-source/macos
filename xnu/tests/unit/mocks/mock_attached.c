/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include "std_safe.h"
#include "dt_proxy.h"
#include "unit_test_utils.h"

#include <mach/arm/vm_types.h>  // for vm_map_offset_t
#include <mach/mach_types.h>
#include <kern/assert.h>
#include <kern/debug.h>

// This file is linked to the same .dylib as the XNU code static library
// to provide some utilities that XNU code relies on when building for unit-test

// This symbol normally comes from lastkernelconstructor.o but this object is not linked to libkernel since it is
// a zero length symbol, which fails to prelink with ld -r
// It is defined here again to resolve the external.
// It is used for debugging and in kext related functions which are not needed by the tester
void* last_kernel_symbol = NULL;

// In normal XNU build this is a global of type mach_header_64 that the linker adds. The user-space linker
// doesn't add it so it's added here to resolve the external. It's not needed by the tester. see <mach-o/ldsyms.h>
int _mh_execute_header;

// called from fake_init, setup proxies for darwintest asserts
struct dt_proxy_callbacks *dt_proxy = NULL;
void
set_dt_proxy_attached(struct dt_proxy_callbacks *p)
{
	dt_proxy = p;
}
struct dt_proxy_callbacks *
get_dt_proxy_attached(void)
{
	return dt_proxy;
}

// check if panic/assert were expected by the test using T_ASSERT_PANIC
struct ut_expected_panic_s ut_expected_panic;

void
ut_check_expected_panic(const char* panic_str)
{
	if (!ut_expected_panic.expect_panic) {
		return;
	}
	ut_expected_panic.expect_panic = false;
	if (ut_expected_panic.str_contains != NULL) {
		if (strstr(panic_str, ut_expected_panic.str_contains) == NULL) {
			PT_LOG_FMTSTR("Panic with unexpected panic-string, expected: `%s`", ut_expected_panic.str_contains);
			return;
		}
	}
	PT_LOG("Panic was expected");
	longjmp(ut_expected_panic.jb, 1);
}

// This function is called on an assert instead of invoking a brk instruction which would trap the kernel
__attribute__((noreturn)) void
ut_assert_trap(int code, long a, long b, long c)
{
	struct kernel_panic_reason pr = {};
	if (code == MACH_ASSERT_TRAP_CODE) {
		panic_assert_format(pr.buf, sizeof(pr.buf), (struct mach_assert_hdr *)a, b, c);
		PT_LOG_OR_RAW_FMTSTR("%s", pr.buf);
	} else {
		snprintf(pr.buf, sizeof(pr.buf), "%x", code);
		PT_LOG_OR_RAW_FMTSTR("Unknown assert code %s", pr.buf);
	}

	ut_check_expected_panic(pr.buf); // may not return
	PT_FAIL("Unexpected assert fail, exiting");
	abort();
}

// This function can be called from the tested code to force a context switch when using fibers
// See the mock implementation
void
ut_fibers_ctxswitch(void)
{
}

// This function can be called from the tested code to force a context switch to a specific fiber
// See the mock implementation
void
ut_fibers_ctxswitch_to(int fiber_id)
{
}

// This function can be called from the tested code to get the current fiber id when using fibers, -1 otherwise
// See the mock implementation
int
ut_fibers_current_id(void)
{
	return -1;
}

static void
fail_not_mocked()
{
	PT_FAIL("This function should never be called since it is mocked by the mocks dylib");
}

// This function is changed from being a macro. It needs to have an implementation
// in the code attached to XNU so that it can be mocked
__mockable void
lock_disable_preemption_for_thread(thread_t t)
{
	fail_not_mocked();
}

__mockable __attribute__((const)) thread_t
current_thread_fast(void)
{
	fail_not_mocked();
	return NULL;
}



extern vm_map_t vm_map_create_external(
	pmap_t                  pmap,
	vm_map_offset_t         min,
	vm_map_offset_t         max,
	boolean_t               pageable);

// this function alias is done during linking of XNU, which doesn't happen when building the library
vm_map_t
vm_map_create(
	pmap_t                  pmap,
	vm_map_offset_t         min,
	vm_map_offset_t         max,
	boolean_t               pageable)
{
	return vm_map_create_external(pmap, min, max, pageable);
}
