/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1989, 1993
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


#include <sys/param.h>
#include <signal.h>
#include <errno.h>

static int sigvec__(signo, sv, osv, bind)
	int signo;
	struct sigvec *sv, *osv;
	int bind;
{
	int ret;

	if (sv)
		sv->sv_flags ^= SV_INTERRUPT;	/* !SA_INTERRUPT */
#if defined(__DYNAMIC__)
        if (bind) {
#endif
            ret = sigaction(signo, (struct sigaction *)sv, (struct sigaction *)osv);
#if defined(__DYNAMIC__)
	} else {
            ret = _sigaction_nobind(signo, (struct sigaction *)sv, (struct sigaction *)osv);
        }
#endif
        if (ret == 0 && osv)
		osv->sv_flags ^= SV_INTERRUPT;	/* !SA_INTERRUPT */
	return (ret);
}

int sigvec(signo, sv, osv)
        int signo;
        struct sigvec *sv, *osv;
{
    return sigvec__(signo, sv, osv, 1);
}

#if defined(__DYNAMIC__)
int _sigvec_nobind(signo, sv, osv)
        int signo;
        struct sigvec *sv, *osv;
{
    return sigvec__(signo, sv, osv, 0);
}
#endif

int sigsetmask(mask)
	int mask;
{
	int omask, n;

	n = sigprocmask(SIG_SETMASK, (sigset_t *) &mask, (sigset_t *) &omask);
	if (n)
		return (n);
	return (omask);
}

int sigblock(mask)
	int mask;
{
	int omask, n;

	n = sigprocmask(SIG_BLOCK, (sigset_t *) &mask, (sigset_t *) &omask);
	if (n)
		return (n);
	return (omask);
}

int sigpause(mask)
	int mask;
{
	return (sigsuspend((sigset_t *)&mask));
}

int sighold(sig)
	int sig;
{
	sigset_t mask;

	if ((sig < 0) || (sig > NSIG))
		return(EINVAL);
	sigemptyset(&mask);
	sigaddset(&mask, sig);
	return(sigprocmask(SIG_BLOCK, &mask,(sigset_t *)0));
}
int sigrelse(sig)
	int sig;
{
	sigset_t mask;

	if ((sig < 0) || (sig > NSIG))
		return(EINVAL);
	sigemptyset(&mask);
	sigaddset(&mask, sig);
	return(sigprocmask(SIG_UNBLOCK, &mask,(sigset_t *)0));
}

