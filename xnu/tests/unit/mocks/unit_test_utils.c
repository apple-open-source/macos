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

#include "unit_test_utils.h"
#include "dt_proxy.h"
#include "sys/queue.h"
#include <string.h>

extern int backtrace(void **array, int size);
extern char **backtrace_symbols(void *const *array, int size);

#ifdef __BUILDING_WITH_SANITIZER__
extern void __sanitizer_symbolize_pc(void *pc, const char *fmt, char *out_buf, size_t out_buf_size);
#endif

extern int kernel_sysctlbyname(const char *, void *, size_t *, void *, size_t);

int64_t
run_sysctl_test(const char *t, int64_t value, int argc, char* const* argv)
{
	char name[1024];
	int64_t result = 0;
	size_t s = sizeof(value);
	int rc;

	snprintf(name, sizeof(name), "debug.test.%s", t);

	bool run_real = (argc > 0 && strcmp(argv[0], "real_sysctl") == 0);
	if (!run_real) {
		rc = kernel_sysctlbyname(name, &result, &s, &value, s);
	} else {
		rc = sysctlbyname(name, &result, &s, &value, s);
	}
	PT_QUIET; PT_ASSERT_POSIX_ZERO(rc, "sysctlbyname()");
	return result;
}

void *
checked_alloc_align(size_t size, size_t align)
{
	void *ptr = NULL;
	if (align < sizeof(void *)) {
		ptr = calloc(1, size);
		PT_QUIET; PT_ASSERT_NOTNULL(ptr, "failed alloc");
	} else {
		ptr = aligned_alloc(align, size);
		PT_QUIET; PT_ASSERT_NOTNULL(ptr, "failed alloc");
		memset(ptr, 0, size);
	}
	return ptr;
}

struct backtrace_array *
collect_current_backtrace(void)
{
	struct backtrace_array *bt = malloc(sizeof(struct backtrace_array));
	bt->nptrs = backtrace(bt->buffer, 100);
	return bt;
}

void
print_collected_backtrace(struct backtrace_array *bt)
{
#ifdef __BUILDING_WITH_SANITIZER__
	// If compiled with any sanitizer, use __sanitizer_symbolize_pc as it gives much more info compared to backtrace_symbols
	char description[1024];
	for (int idx = 0; idx < bt->nptrs; idx++) {
		__sanitizer_symbolize_pc(bt->buffer[idx], "%p %F %L", description,
		    sizeof(description));
		raw_printf("%d\t%s\n", idx, description);
	}
#else
	char** strings = backtrace_symbols(bt->buffer, bt->nptrs);
	PT_QUIET; PT_ASSERT_NOTNULL(strings, "backtrace_symbols");
	for (int idx = 0; idx < bt->nptrs; idx++) {
		raw_printf("%s\n", strings[idx]);
	}
	free(strings);
#endif
	raw_printf("\n");
}

void
print_current_backtrace(void)
{
	struct backtrace_array bt;
	bt.nptrs = backtrace(bt.buffer, 100);
	print_collected_backtrace(&bt);
}
