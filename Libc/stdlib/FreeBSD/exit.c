/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)exit.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdlib/exit.c,v 1.9 2007/01/09 00:28:09 imp Exp $");

#include "namespace.h"
#include <stdlib.h>
#include <unistd.h>
#include "un-namespace.h"

#include "atexit.h"
#include "libc_private.h"
#include "stdio/FreeBSD/local.h" // for __cleanup

#include <TargetConditionals.h>

#if __APPLE__
int __cleanup = 0;
#else
void (* CLEANUP_PTRAUTH __cleanup)(void);
#endif // __APPLE__

extern void __exit(int) __attribute__((noreturn));
#if __has_feature(cxx_thread_local) || __has_feature(c_thread_local)
extern void _tlv_exit();
#endif // __has_feature(cxx_thread_local) || __has_feature(c_thread_local)

/*
 * Exit, flushing stdio buffers if necessary.
 */
void
exit(int status)
{
#if __has_feature(cxx_thread_local) || __has_feature(c_thread_local)
	_tlv_exit(); // C++11 requires thread_local objects to be destroyed before global objects
#endif // __has_feature(cxx_thread_local) || __has_feature(c_thread_local)
	__cxa_finalize(NULL);
	if (__cleanup)
#ifdef __APPLE__
		_cleanup();
#else
		(*__cleanup)();
#endif // __APPLE__
	__exit(status);
}
#pragma clang diagnostic pop
