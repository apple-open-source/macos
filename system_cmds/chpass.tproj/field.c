/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1988, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "chpass.h"
#include "pathnames.h"

/* ARGSUSED */
int
p_login(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p) {
		warnx("empty login field");
		return (1);
	}
	if (*p == '-') {
		warnx("login names may not begin with a hyphen");
		return (1);
	}
	if (!(pw->pw_name = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	if (strchr(p, '.'))
		warnx("\'.\' is dangerous in a login name");
	for (; *p; ++p)
		if (isupper(*p)) {
			warnx("upper-case letters are dangerous in a login name");
			break;
		}
	return (0);
}

/* ARGSUSED */
int
p_passwd(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		pw->pw_passwd = "";	/* "NOLOGIN"; */
	else if (!(pw->pw_passwd = strdup(p))) {
		warnx("can't save password entry");
		return (1);
	}
	
	return (0);
}

/* ARGSUSED */
int
p_uid(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	uid_t id;
	char *np;

	if (!*p) {
		warnx("empty uid field");
		return (1);
	}
	if (!isdigit(*p)) {
		warnx("illegal uid");
		return (1);
	}
	errno = 0;
	id = strtoul(p, &np, 10);
	if (*np || (id == ULONG_MAX && errno == ERANGE)) {
		warnx("illegal uid");
		return (1);
	}
	pw->pw_uid = id;
	return (0);
}

/* ARGSUSED */
int
p_gid(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	struct group *gr;
	gid_t id;
	char *np;

	if (!*p) {
		warnx("empty gid field");
		return (1);
	}
	if (!isdigit(*p)) {
		if (!(gr = getgrnam(p))) {
			warnx("unknown group %s", p);
			return (1);
		}
		pw->pw_gid = gr->gr_gid;
		return (0);
	}
	errno = 0;
	id = strtoul(p, &np, 10);
	if (*np || (id == ULONG_MAX && errno == ERANGE)) {
		warnx("illegal gid");
		return (1);
	}
	pw->pw_gid = id;
	return (0);
}

/* ARGSUSED */
int
p_class(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		pw->pw_class = "";
	else if (!(pw->pw_class = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	
	return (0);
}

/* ARGSUSED */
int
p_change(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!atot(p, &pw->pw_change))
		return (0);
	warnx("illegal date for change field");
	return (1);
}

/* ARGSUSED */
int
p_expire(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!atot(p, &pw->pw_expire))
		return (0);
	warnx("illegal date for expire field");
	return (1);
}

/* ARGSUSED */
int
p_gecos(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		ep->save = "";
	else if (!(ep->save = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_hdir(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p) {
		warnx("empty home directory field");
		return (1);
	}
	if (!(pw->pw_dir = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_shell(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	char *t, *ok_shell();

	if (!*p) {
		pw->pw_shell = _PATH_BSHELL;
		return (0);
	}
	/* only admin can change from or to "restricted" shells */
	if (uid && pw->pw_shell && !ok_shell(pw->pw_shell)) {
		warnx("%s: current shell non-standard", pw->pw_shell);
		return (1);
	}
	if (!(t = ok_shell(p))) {
		if (uid) {
			warnx("%s: non-standard shell", p);
			return (1);
		}
	}
	else
		p = t;
	if (!(pw->pw_shell = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

#ifdef DIRECTORY_SERVICE
void
d_change(struct display *d, FILE *fp)
{
	fprintf(fp, "Change [month day year]: %s\n", ttoa(d->pw->pw_change));
}

void
d_class(struct display *d, FILE *fp)
{
	fprintf(fp, "Class: %s\n", d->pw->pw_class);
}

void
d_expire(struct display *d, FILE *fp)
{
	fprintf(fp, "Expire [month day year]: %s\n", ttoa(d->pw->pw_expire));
}

void
d_fullname(struct display *d, FILE *fp)
{
	fprintf(fp, "Full Name: %s\n", d->fullname);
}

void
d_gid(struct display *d, FILE *fp)
{
	fprintf(fp, "Gid [# or name]: %d\n", d->pw->pw_gid);
}

void
d_hdir(struct display *d, FILE *fp)
{
	fprintf(fp, "Home directory: %s\n", d->pw->pw_dir);
}

void
d_homephone(struct display *d, FILE *fp)
{
	fprintf(fp, "Home Phone: %s\n", d->homephone);
}

void
d_login(struct display *d, FILE *fp)
{
	fprintf(fp, "Login: %s\n", d->pw->pw_name);
}

void
d_location(struct display *d, FILE *fp)
{
	fprintf(fp, "Location: %s\n", d->location);
}

void
d_officephone(struct display *d, FILE *fp)
{
	fprintf(fp, "Office Phone: %s\n", d->officephone);
}

void
d_passwd(struct display *d, FILE *fp)
{
	fprintf(fp, "Password: %s\n", d->pw->pw_passwd);
}

void
d_shell(struct display *d, FILE *fp)
{
	fprintf(fp, "Shell: %s\n", *d->pw->pw_shell ? d->pw->pw_shell
		: _PATH_BSHELL);
}

void
d_uid(struct display *d, FILE *fp)
{
	fprintf(fp, "Uid [#]: %d\n", d->pw->pw_uid);
}
#endif /* DIRECTORY_SERVICE */
