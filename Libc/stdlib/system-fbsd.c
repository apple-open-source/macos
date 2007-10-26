/*
 * Copyright (c) 1988, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)system.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdlib/system.c,v 1.10 2002/03/22 21:53:10 obrien Exp $");

#include "namespace.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <paths.h>
#include <errno.h>
#include "un-namespace.h"
#include "libc_private.h"

#if __DARWIN_UNIX03
#include <pthread.h>

static pthread_mutex_t __systemfn_mutex = PTHREAD_MUTEX_INITIALIZER;
extern int __unix_conforming;
#ifdef VARIANT_CANCELABLE
extern void _pthread_testcancel(pthread_t thread, int isconforming);
#endif /* VARIANT_CANCELABLE */
#endif /* __DARWIN_UNIX03 */

int
__system(command)
	const char *command;
{
	pid_t pid, savedpid;
	int pstat;
	struct sigaction ign, intact, quitact;
	sigset_t newsigblock, oldsigblock;

#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
#ifdef VARIANT_CANCELABLE
	_pthread_testcancel(pthread_self(), 1);
#endif /* VARIANT_CANCELABLE */
#endif /* __DARWIN_UNIX03 */

	if (!command) {		/* just checking... */
		if (access(_PATH_BSHELL, F_OK) == -1)	/* if no sh or no access */
			return(0);
		else
			return(1);
	}

#if __DARWIN_UNIX03
	pthread_mutex_lock(&__systemfn_mutex);
#endif /* __DARWIN_UNIX03 */
	/*
	 * Ignore SIGINT and SIGQUIT, block SIGCHLD. Remember to save
	 * existing signal dispositions.
	 */
	ign.sa_handler = SIG_IGN;
	(void)sigemptyset(&ign.sa_mask);
	ign.sa_flags = 0;
	(void)_sigaction(SIGINT, &ign, &intact);
	(void)_sigaction(SIGQUIT, &ign, &quitact);
	(void)sigemptyset(&newsigblock);
	(void)sigaddset(&newsigblock, SIGCHLD);
	(void)_sigprocmask(SIG_BLOCK, &newsigblock, &oldsigblock);
	switch(pid = fork()) {
	case -1:			/* error */
		break;
	case 0:				/* child */
		/*
		 * Restore original signal dispositions and exec the command.
		 */
		(void)_sigaction(SIGINT, &intact, NULL);
		(void)_sigaction(SIGQUIT,  &quitact, NULL);
		(void)_sigprocmask(SIG_SETMASK, &oldsigblock, NULL);
#if __DARWIN_UNIX03
		pthread_mutex_unlock(&__systemfn_mutex);
#endif /* __DARWIN_UNIX03 */
		execl(_PATH_BSHELL, "sh", "-c", command, (char *)NULL);
		_exit(127);
	default:			/* parent */
		savedpid = pid;
		do {
			pid = _wait4(savedpid, &pstat, 0, (struct rusage *)0);
		} while (pid == -1 && errno == EINTR);
		break;
	}
	(void)_sigaction(SIGINT, &intact, NULL);
	(void)_sigaction(SIGQUIT,  &quitact, NULL);
	(void)_sigprocmask(SIG_SETMASK, &oldsigblock, NULL);
#if __DARWIN_UNIX03
	pthread_mutex_unlock(&__systemfn_mutex);
#endif /* __DARWIN_UNIX03 */
	return(pid == -1 ? -1 : pstat);
}

__weak_reference(__system, system);
__weak_reference(__system, _system);
