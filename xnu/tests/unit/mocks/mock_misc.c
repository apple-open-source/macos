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

#include "mock_misc.h"
#include "std_safe.h"
#include "unit_test_utils.h"
#include "dt_proxy.h"

#include "fibers/random.h"

#include <kern/btlog.h>
#include <mach/vm_types.h>
#include <vm/vm_sanitize_telemetry.h>

// This initialized the darwintest asserts proxies in the mocks .dylib
struct dt_proxy_callbacks *dt_proxy = NULL;
void
set_dt_proxy_mock(struct dt_proxy_callbacks *p)
{
	dt_proxy = p;
}
struct dt_proxy_callbacks *
get_dt_proxy_mock(void)
{
	return dt_proxy;
}


// for cpu_data_startup_init
T_MOCK(unsigned int,
ml_get_cpu_count, (void))
{
	return 1;
}

T_MOCK(vm_offset_t,
min_valid_stack_address, (void))
{
	return 0;
}

T_MOCK(vm_offset_t,
max_valid_stack_address, (void))
{
	return 0;
}

T_MOCK(u_int32_t,
RandomULong, (void))
{
	return (u_int32_t)random_next();
}

T_MOCK(uint64_t,
early_random, (void))
{
	return random_next();
}

// needed because in-kernel impl for some reason got to libcorecrypt dyld
T_MOCK(void,
read_erandom, (void * buffer, unsigned int numBytes))
{
	unsigned char *cbuf = (unsigned char *)buffer;
	for (int i = 0; i < numBytes; ++i) {
		cbuf[i] = (unsigned char)(random_next() % 0xFF);
	}
}

T_MOCK(void,
read_random, (void * buffer, unsigned int numbytes))
{
	read_erandom(buffer, numbytes);
}

T_MOCK(uint32_t,
PE_get_random_seed, (unsigned char *dst_random_seed, uint32_t request_size))
{
	for (uint32_t i = 0; i < request_size; i++, dst_random_seed++) {
		*dst_random_seed = 0;
	}
	return request_size;
}

T_MOCK(bool,
ml_unsafe_kernel_text, (void))
{
	return true;
}


T_MOCK(__attribute__((noinline, not_tail_called)) void,
os_log_with_args, (void* oslog, uint8_t type, const char *fmt, va_list args, void *addr))
{
	char buf[PRINT_BUF_SIZE];
	int printed = vsnprintf(buf, PRINT_BUF_SIZE, fmt, args);
	if (printed > PRINT_BUF_SIZE - 1) {
		printed = PRINT_BUF_SIZE - 1;
	}
#if 0  // this can be switched on if we want pre-main logs
	buf[printed] = '\n';
	write(STDOUT_FILENO, buf, printed);
#else
	PT_LOG(buf);
#endif
}


// The panic() mock works in conjunction with T_ASSERT_PANIC()
// XNU code that panics doesn't expect panic() to return so any function that calls panic() doesn't bother
// to return gracefully to its caller with an error.
// In a unit-test we still want to call a function that is expected to panic, and then be able to run code after it.
// T_ASSERT_PANIC creates a setjmp() point before the call that is expected to panic.
// Once the panic callback panic_trap_to_debugger() is called it does a longjmp() to that jump point.
// This has a similar effect as C++ exceptions, except that any memory allocations performed by the code
// prior to the panic are going to be leaked.

T_MOCK(void,
panic_trap_to_debugger, (const char *panic_format_str, va_list * panic_args,
unsigned int reason, void *ctx, uint64_t panic_options_mask, void *panic_data,
unsigned long panic_caller, const char *panic_initiator))
{
	char buf[PRINT_BUF_SIZE];
	vsnprintf(buf, PRINT_BUF_SIZE, panic_format_str, *panic_args);
	PT_LOG_OR_RAW_FMTSTR("panic! %s", buf);
	ut_check_expected_panic(buf); // may not return
	PT_FAIL("Panic was unexpected, exiting");
	abort();
}

T_MOCK(void,
vm_sanitize_send_telemetry, (
	vm_sanitize_method_t method,
	vm_sanitize_checker_t checker,
	vm_sanitize_checker_count_t checker_count,
	enum vm_sanitize_subsys_error_codes ktriage_code,
	uint64_t arg1,
	uint64_t arg2,
	uint64_t arg3,
	uint64_t arg4,
	uint64_t future_ret,
	uint64_t past_ret))
{
}

#if (DEBUG || DEVELOPMENT)

T_MOCK(vm_size_t,
zone_element_info, (
	void *addr,
	vm_tag_t * ptag))
{
	return 0;
}

#endif // DEBUG || DEVELOPMENT

// added for setup_nested_submap()
T_MOCK(kern_return_t,
csm_setup_nested_address_space, (
	pmap_t pmap,
	const vm_address_t region_addr,
	const vm_size_t region_size))
{
	return KERN_SUCCESS;
}

T_MOCK(btref_t,
btref_get, (
	void *fp,
	btref_get_flags_t flags))
{
	return 0;
}

#if (DEBUG || DEVELOPMENT)
// these are used for testing the mocking framework, xnu has them only in development || debug
T_MOCK_DYNAMIC(size_t, kernel_func1, (int a, char b), (a, b), { return 0; });
T_MOCK_DYNAMIC(size_t, kernel_func2, (int a, char b), (a, b), { return 0; });
T_MOCK_DYNAMIC(size_t, kernel_func3, (int a, char b), (a, b), { return 0; });
T_MOCK_DYNAMIC(size_t, kernel_func4, (int a, char b), (a, b), { return 0; });
T_MOCK_DYNAMIC(size_t, kernel_func5, (int a, char b), (a, b), { return kernel_func5(a, b); });
T_MOCK_DYNAMIC(void, kernel_func6, (int a, char b), (a, b), { kernel_func6(a, b); });
T_MOCK_DYNAMIC(size_t, kernel_func7, (int a, char b), (a, b));
T_MOCK_DYNAMIC(void, kernel_func8, (int a, char b), (a, b));
#endif // DEBUG || DEVELOPMENT
