/*	$OpenBSD: passwd.c,v 1.42 2003/06/26 16:34:42 deraadt Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
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
 * 3. Neither the name of the University nor the names of its contributors
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
static const char rcsid[] = "$OpenBSD: passwd.c,v 1.42 2003/06/26 16:34:42 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <limits.h>

#include "util.h"

int
pw_scan(char *bp, struct passwd *pw, int *flags)
{
	u_long id;
	int root;
	char *p, *sh, *p2;

	if (flags != (int *)NULL)
		*flags = 0;

#ifdef __APPLE__
	if (bp[0] == '#') {
		pw->pw_name = NULL;
		return(1);
	}
#endif

	if (!(p = strsep(&bp, ":")) || *p == '\0')	/* login */
		goto fmt;
	pw->pw_name = p;
	root = !strcmp(pw->pw_name, "root");

	if (!(pw->pw_passwd = strsep(&bp, ":")))	/* passwd */
		goto fmt;

	if (!(p = strsep(&bp, ":")))			/* uid */
		goto fmt;
	id = strtoul(p, &p2, 10);
	if (root && id) {
		warnx("root uid should be 0");
		return (0);
	}
	if (*p2 != '\0') {
		warnx("illegal uid field");
		return (0);
	}
#ifndef __APPLE__
	/* Apple's UID_MAX is too small (sizeof signed) 3091256 */
	if (id > UID_MAX) {
		/* errno is set to ERANGE by strtoul(3) */
		warnx("uid greater than %u", UID_MAX-1);
		return (0);
	}
#endif
	pw->pw_uid = (uid_t)id;
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOUID;

	if (!(p = strsep(&bp, ":")))			/* gid */
		goto fmt;
	id = strtoul(p, &p2, 10);
	if (*p2 != '\0') {
		warnx("illegal gid field");
		return (0);
	}
#ifndef __APPLE__
	/* Apple's UID_MAX is too small (sizeof signed) 3091256 */
	if (id > UID_MAX) {
		/* errno is set to ERANGE by strtoul(3) */
		warnx("gid greater than %u", UID_MAX-1);
		return (0);
	}
#endif
	pw->pw_gid = (gid_t)id;
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOGID;

	pw->pw_class = strsep(&bp, ":");		/* class */
	if (!(p = strsep(&bp, ":")))			/* change */
		goto fmt;
	pw->pw_change = atol(p);
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOCHG;
	if (!(p = strsep(&bp, ":")))			/* expire */
		goto fmt;
	pw->pw_expire = atol(p);
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOEXP;
	pw->pw_gecos = strsep(&bp, ":");		/* gecos */
	pw->pw_dir = strsep(&bp, ":");			/* directory */
	if (!(pw->pw_shell = strsep(&bp, ":")))		/* shell */
		goto fmt;

	p = pw->pw_shell;
	if (root && *p) {				/* empty == /bin/sh */
		for (setusershell();;) {
			if (!(sh = getusershell())) {
				warnx("warning, unknown root shell");
				break;
			}
			if (!strcmp(p, sh))
				break;
		}
		endusershell();
	}

	if ((p = strsep(&bp, ":"))) {			/* too many */
fmt:		warnx("corrupted entry");
		return (0);
	}

	return (1);
}
