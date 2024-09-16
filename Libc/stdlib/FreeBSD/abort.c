/*
 * Copyright (c) 1985, 1993
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
static char sccsid[] = "@(#)abort.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdlib/abort.c,v 1.11 2007/01/09 00:28:09 imp Exp $");

#include "namespace.h"
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <pthread_workqueue.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "stdio/FreeBSD/local.h" // for __cleanup

#if __has_include(<CrashReporterClient.h>)
#include <CrashReporterClient.h>
#else
#define CRGetCrashLogMessage() NULL
#define CRSetCrashLogMessage(...)
#endif
#include "_simple.h"

extern void __abort(void) __cold __dead2;

#define TIMEOUT	10000	/* 10 milliseconds */

void
abort()
{
	struct sigaction act;

	if (!CRGetCrashLogMessage())
		CRSetCrashLogMessage("abort() called");

	/*
	 * Fetch pthread_self() now, before we start masking signals.
	 * pthread_self will abort or crash if the pthread's signature
	 * appears corrupt. aborting inside abort is painful, so let's get
	 * that out of the way before we go any further.
	 */
	pthread_t self = pthread_self();

	/*
	 * POSIX requires we flush stdio buffers on abort.
	 * XXX ISO C requires that abort() be async-signal-safe.
	 */
	if (__cleanup)
#ifdef __APPLE__
		_cleanup();
#else
	    (*__cleanup)();
#endif // __APPLE__

	sigfillset(&act.sa_mask);
	/*
	 * Don't block SIGABRT to give any handler a chance; we ignore
	 * any errors -- ISO C doesn't allow abort to return anyway.
	 */
	sigdelset(&act.sa_mask, SIGABRT);

	/*
	 * Don't block SIGSEGV since we might trigger a segfault if the pthread
	 * struct is corrupt. The end user behavior is that the program will
	 * terminate with a SIGSEGV instead of a SIGABRT which is acceptable. If
	 * the user registers a SIGSEGV handler, then they are responsible for
	 * dealing with any corruption themselves and abort may not work.
	 * rdar://48853131
	 */
	sigdelset(&act.sa_mask, SIGSEGV);
	sigdelset(&act.sa_mask, SIGBUS);

	/* <rdar://problem/7397932> abort() should call pthread_kill to deliver a signal to the aborting thread 
	 * This helps gdb focus on the thread calling abort()
	 */

	/* Block all signals on all other threads */
	sigset_t fullmask;
	sigfillset(&fullmask);
	(void)_sigprocmask(SIG_SETMASK, &fullmask, NULL);

	/* <rdar://problem/8400096> Set the workqueue killable */
	__pthread_workqueue_setkill(1);

	(void)pthread_sigmask(SIG_SETMASK, &act.sa_mask, NULL);
	(void)pthread_kill(self, SIGABRT);

	usleep(TIMEOUT); /* give time for signal to happen */

	/*
	 * If SIGABRT was ignored, or caught and the handler returns, do
	 * it again, only harder.
	 */
	 __abort();
}

__private_extern__ void
__abort()
{
	struct sigaction act;

	if (!CRGetCrashLogMessage())
		CRSetCrashLogMessage("__abort() called");

	/* Fetch pthread_self() before masking signals - see above. */
	pthread_t self = pthread_self();

	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;
	sigfillset(&act.sa_mask);
	(void)_sigaction(SIGABRT, &act, NULL);
	sigdelset(&act.sa_mask, SIGABRT);

	/* <rdar://problem/7397932> abort() should call pthread_kill to deliver a signal to the aborting thread 
	 * This helps gdb focus on the thread calling abort()
	 */

	/* Block all signals on all other threads */
	sigset_t fullmask;
	sigfillset(&fullmask);
	(void)_sigprocmask(SIG_SETMASK, &fullmask, NULL);

	/* <rdar://problem/8400096> Set the workqueue killable */
	__pthread_workqueue_setkill(1);

	(void)pthread_sigmask(SIG_SETMASK, &act.sa_mask, NULL);
	(void)pthread_kill(self, SIGABRT);

	usleep(TIMEOUT); /* give time for signal to happen */

	/* If for some reason SIGABRT was not delivered, we exit using __builtin_trap
	 * which generates an illegal instruction on i386: <rdar://problem/8400958>
	 * and SIGTRAP on arm.
	 */
	sigfillset(&act.sa_mask);
	sigdelset(&act.sa_mask, SIGILL);
	sigdelset(&act.sa_mask, SIGTRAP);
	(void)_sigprocmask(SIG_SETMASK, &act.sa_mask, NULL);
	__builtin_trap();
}

void
abort_report_np(const char *fmt, ...)
{
	_SIMPLE_STRING s;
	va_list ap;

	if ((s = _simple_salloc()) != NULL) {
		va_start(ap, fmt);
		_simple_vsprintf(s, fmt, ap);
		va_end(ap);
		CRSetCrashLogMessage(_simple_string(s));
	} else
		CRSetCrashLogMessage(fmt); /* the format string is better than nothing */
	abort();
}
#pragma clang diagnostic pop
