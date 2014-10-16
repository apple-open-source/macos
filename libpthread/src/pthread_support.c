/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#include "internal.h"
#include <dlfcn.h>
#include <_simple.h>
#include <CrashReporterClient.h>

#define __SIGABRT 6

/* We should move abort() into Libsyscall, if possible. */
int __getpid(void);

PTHREAD_NORETURN int
__kill(int pid, int signum, int posix);

void
__pthread_abort(void)
{
	PTHREAD_NORETURN void (*_libc_abort)(void);
	_libc_abort = dlsym(RTLD_DEFAULT, "abort");

	if (_libc_abort) {
		_libc_abort();
	} else {
		__kill(__getpid(), __SIGABRT, 0);
	}
}

void
__pthread_abort_reason(const char *fmt, ...)
{
#if !TARGET_OS_EMBEDDED
	va_list ap;
	const char *str = fmt;
	_SIMPLE_STRING s = _simple_salloc();
	va_start(ap, fmt);
	if (_simple_vsprintf(s, fmt, ap) == 0) {
		str = _simple_string(s);
	}
	CRSetCrashLogMessage(str);
	va_end(ap);
#endif
	__pthread_abort();
}
