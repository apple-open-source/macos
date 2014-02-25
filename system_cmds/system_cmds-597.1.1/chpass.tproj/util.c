/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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

#if 0
#ifndef lint
#if 0
static char sccsid[] = "@(#)util.c	8.4 (Berkeley) 4/2/94";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/chpass/util.c,v 1.13 2004/01/18 21:46:39 charnier Exp $");
#endif

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

#include "chpass.h"

#if OPEN_DIRECTORY
#include <err.h>
#include <paths.h>
#include <sys/stat.h>
#include "open_directory.h"

char* tempname;
#endif /* OPEN_DIRECTORY */

static const char *months[] =
	{ "January", "February", "March", "April", "May", "June",
	  "July", "August", "September", "October", "November",
	  "December", NULL };

char *
ttoa(time_t tval)
{
	struct tm *tp;
	static char tbuf[50];

	if (tval) {
		tp = localtime(&tval);
		(void)sprintf(tbuf, "%s %d, %d", months[tp->tm_mon],
		    tp->tm_mday, tp->tm_year + TM_YEAR_BASE);
	}
	else
		*tbuf = '\0';
	return (tbuf);
}

int
atot(char *p, time_t *store)
{
	static struct tm *lt;
	char *t;
	const char **mp;
	time_t tval;
	int day, month, year;

	if (!*p) {
		*store = 0;
		return (0);
	}
	if (!lt) {
		unsetenv("TZ");
		(void)time(&tval);
		lt = localtime(&tval);
	}
	if (!(t = strtok(p, " \t")))
		goto bad;
	if (isdigit(*t)) {
		month = atoi(t);
	} else {
		for (mp = months;; ++mp) {
			if (!*mp)
				goto bad;
			if (!strncasecmp(*mp, t, 3)) {
				month = mp - months + 1;
				break;
			}
		}
	}
	if (!(t = strtok((char *)NULL, " \t,")) || !isdigit(*t))
		goto bad;
	day = atoi(t);
	if (!(t = strtok((char *)NULL, " \t,")) || !isdigit(*t))
		goto bad;
	year = atoi(t);
	if (day < 1 || day > 31 || month < 1 || month > 12)
		goto bad;
	/* Allow two digit years 1969-2068 */
	if (year < 69)
		year += 2000;
	else if (year < 100)
		year += TM_YEAR_BASE;
	if (year < EPOCH_YEAR)
bad:		return (1);
	lt->tm_year = year - TM_YEAR_BASE;
	lt->tm_mon = month - 1;
	lt->tm_mday = day;
	lt->tm_hour = 0;
	lt->tm_min = 0;
	lt->tm_sec = 0;
	lt->tm_isdst = -1;
	if ((tval = mktime(lt)) < 0)
		return (1);
	*store = tval;
	return (0);
}

int
ok_shell(char *name)
{
#ifdef __APPLE__
	char *sh;
#else
	char *p, *sh;
#endif

	setusershell();
	while ((sh = getusershell())) {
		if (!strcmp(name, sh)) {
			endusershell();
			return (1);
		}
#ifndef __APPLE__
		/* allow just shell name, but use "real" path */
		if ((p = strrchr(sh, '/')) && strcmp(name, p + 1) == 0) {
			endusershell();
			return (1);
		}
#endif
	}
	endusershell();
	return (0);
}

char *
dup_shell(char *name)
{
	char *p, *sh, *ret;

	setusershell();
	while ((sh = getusershell())) {
		if (!strcmp(name, sh)) {
			endusershell();
			return (strdup(name));
		}
		/* allow just shell name, but use "real" path */
		if ((p = strrchr(sh, '/')) && strcmp(name, p + 1) == 0) {
			ret = strdup(sh);
			endusershell();
			return (ret);
		}
	}
	endusershell();
	return (NULL);
}

#if OPEN_DIRECTORY
int
cfprintf(FILE* file, const char* format, ...) {
		char* cstr;
		int result = 0;
        va_list args;
        va_start(args, format);
        CFStringRef formatStr = CFStringCreateWithCStringNoCopy(NULL, format, kCFStringEncodingUTF8, kCFAllocatorNull);
		if (formatStr) {
			CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, formatStr, args);
			if (str) {
				size_t size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
				va_end(args);
				cstr = malloc(size);
				if (cstr && CFStringGetCString(str, cstr, size, kCFStringEncodingUTF8)) {
					result = fprintf(file, "%s", cstr);
					free(cstr);
				}
				CFRelease(str);
			}
			CFRelease(formatStr);
		}
		return result;
}

/*
 * Edit the temp file.  Return -1 on error, >0 if the file was modified, 0
 * if it was not.
 */
int
editfile(const char* tfn)
{
	struct sigaction sa, sa_int, sa_quit;
	sigset_t oldsigset, sigset;
	struct stat st1, st2;
	const char *p, *editor;
	int pstat;
	pid_t editpid;

	if ((editor = getenv("EDITOR")) == NULL)
		editor = _PATH_VI;
	if (p = strrchr(editor, '/'))
		++p;
	else 
		p = editor;

	if (stat(tfn, &st1) == -1)
		return (-1);
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &sa_int);
	sigaction(SIGQUIT, &sa, &sa_quit);
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sigset, &oldsigset);
	switch ((editpid = fork())) {
	case -1:
		return (-1);
	case 0:
		sigaction(SIGINT, &sa_int, NULL);
		sigaction(SIGQUIT, &sa_quit, NULL);
		sigprocmask(SIG_SETMASK, &oldsigset, NULL);
		errno = 0;
		if (!master_mode) {
			(void)setgid(getgid());
			(void)setuid(getuid());
		}
		execlp(editor, p, tfn, (char *)NULL);
		_exit(errno);
	default:
		/* parent */
		break;
	}
	for (;;) {
		if (waitpid(editpid, &pstat, WUNTRACED) == -1) {
			if (errno == EINTR)
				continue;
			unlink(tfn);
			editpid = -1;
			break;
		} else if (WIFSTOPPED(pstat)) {
			raise(WSTOPSIG(pstat));
		} else if (WIFEXITED(pstat) && WEXITSTATUS(pstat) == 0) {
			editpid = -1;
			break;
		} else {
			unlink(tfn);
			editpid = -1;
			break;
		}
	}
	sigaction(SIGINT, &sa_int, NULL);
	sigaction(SIGQUIT, &sa_quit, NULL);
	sigprocmask(SIG_SETMASK, &oldsigset, NULL);
	if (stat(tfn, &st2) == -1)
		return (-1);
	return (st1.st_mtime != st2.st_mtime);
}

void
pw_error(char *name, int err, int eval)
{
	if (err)
		warn("%s", name);
	exit(eval);
}

#endif /* OPEN_DIRECTORY */
