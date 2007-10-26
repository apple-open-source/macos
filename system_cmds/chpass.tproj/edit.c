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
#if 0
#ifndef lint
static char sccsid[] = "@(#)edit.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/chpass/edit.c,v 1.23 2003/04/09 18:18:42 des Exp $");
#endif

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef OPEN_DIRECTORY
#include <pw_scan.h>
#include <libutil.h>
#endif

#include "chpass.h"

#ifdef OPEN_DIRECTORY
static int display(const char *tfn, CFDictionaryRef attrs);
static CFDictionaryRef verify(const char *tfn, CFDictionaryRef attrs);
#else
static int display(const char *tfn, struct passwd *pw);
static struct passwd *verify(const char *tfn, struct passwd *pw);
#endif

#ifdef OPEN_DIRECTORY
CFDictionaryRef
edit(const char *tfn, CFDictionaryRef pw)
#else
struct passwd *
edit(const char *tfn, struct passwd *pw)
#endif
{
#ifdef OPEN_DIRECTORY
	CFDictionaryRef npw;
#else
	struct passwd *npw;
#endif
	char *line;
	size_t len;

	if (display(tfn, pw) == -1)
		return (NULL);
	for (;;) {
#ifdef OPEN_DIRECTORY
		switch (editfile(tfn)) {
#else
		switch (pw_edit(1)) {
#endif
		case -1:
			return (NULL);
		case 0:
#ifdef OPEN_DIRECTORY
			return (NULL);
#else
			return (pw_dup(pw));
#endif
		default:
			break;
		}
		if ((npw = verify(tfn, pw)) != NULL)
			return (npw);
#ifndef OPEN_DIRECTORY
		free(npw);
#endif
		printf("re-edit the password file? ");
		fflush(stdout);
		if ((line = fgetln(stdin, &len)) == NULL) {
			warn("fgetln()");
			return (NULL);
		}
		if (len > 0 && (*line == 'N' || *line == 'n'))
			return (NULL);
	}
}

/*
 * display --
 *	print out the file for the user to edit; strange side-effect:
 *	set conditional flag if the user gets to edit the shell.
 */
#if OPEN_DIRECTORY
static int
display(const char *tfn, CFDictionaryRef attrs)
#else
static int
display(const char *tfn, struct passwd *pw)
#endif
{
	FILE *fp;
#ifndef OPEN_DIRECTORY
	char *bp, *gecos, *p;
#endif

	if ((fp = fopen(tfn, "w")) == NULL) {
		warn("%s", tfn);
		return (-1);
	}

#ifdef OPEN_DIRECTORY
	CFArrayRef values = CFDictionaryGetValue(attrs, CFSTR(kDSNAttrRecordName));
	CFStringRef username = (values && CFArrayGetCount(values)) > 0 ? CFArrayGetValueAtIndex(values, 0) : NULL;

	(void)cfprintf(fp,
		"# Changing user information for %@.\n"
	    "# Use \"passwd\" to change the password.\n"
	    "##\n"
		"# Open Directory%s%@\n"
		"##\n",
		username,
		DSPath ? ": " : "",
		DSPath ? DSPath : CFSTR(""));

	int ndisplayed = 0;
	ENTRY* ep;
	for (ep = list; ep->prompt; ep++)
		if (!ep->restricted) {
			ep->display(attrs, ep->attrName, ep->prompt, fp);
			ndisplayed++;
		}
	if(!ndisplayed) {
		(void)fprintf(fp, "###################################\n");
		(void)fprintf(fp, "# No fields are available to change\n");
		(void)fprintf(fp, "###################################\n");
	}
#else OPEN_DIRECTORY
	(void)fprintf(fp,
	    "#Changing user information for %s.\n", pw->pw_name);
	if (master_mode) {
		(void)fprintf(fp, "Login: %s\n", pw->pw_name);
		(void)fprintf(fp, "Password: %s\n", pw->pw_passwd);
		(void)fprintf(fp, "Uid [#]: %lu\n", (unsigned long)pw->pw_uid);
		(void)fprintf(fp, "Gid [# or name]: %lu\n",
		    (unsigned long)pw->pw_gid);
		(void)fprintf(fp, "Change [month day year]: %s\n",
		    ttoa(pw->pw_change));
		(void)fprintf(fp, "Expire [month day year]: %s\n",
		    ttoa(pw->pw_expire));
		(void)fprintf(fp, "Class: %s\n", pw->pw_class);
		(void)fprintf(fp, "Home directory: %s\n", pw->pw_dir);
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	}
	/* Only admin can change "restricted" shells. */
#if 0
	else if (ok_shell(pw->pw_shell))
		/*
		 * Make shell a restricted field.  Ugly with a
		 * necklace, but there's not much else to do.
		 */
#else
	else if ((!list[E_SHELL].restricted && ok_shell(pw->pw_shell)) ||
	    master_mode)
		/*
		 * If change not restrict (table.c) and standard shell
		 *	OR if root, then allow editing of shell.
		 */
#endif
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	else
		list[E_SHELL].restricted = 1;

	if ((bp = gecos = strdup(pw->pw_gecos)) == NULL) {
		warn(NULL);
		fclose(fp);
		return (-1);
	}

	p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_NAME].save = p;
	if (!list[E_NAME].restricted || master_mode)
	  (void)fprintf(fp, "Full Name: %s\n", p);

	p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_LOCATE].save = p;
	if (!list[E_LOCATE].restricted || master_mode)
	  (void)fprintf(fp, "Office Location: %s\n", p);

	p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_BPHONE].save = p;
	if (!list[E_BPHONE].restricted || master_mode)
	  (void)fprintf(fp, "Office Phone: %s\n", p);

	p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_HPHONE].save = p;
	if (!list[E_HPHONE].restricted || master_mode)
	  (void)fprintf(fp, "Home Phone: %s\n", p);

	bp = strdup(bp ? bp : "");
	list[E_OTHER].save = bp;
	if (!list[E_OTHER].restricted || master_mode)
	  (void)fprintf(fp, "Other information: %s\n", bp);

	free(gecos);
#endif /* OPEN_DIRECTORY */

	(void)fchown(fileno(fp), getuid(), getgid());
	(void)fclose(fp);
	return (0);
}

#ifdef OPEN_DIRECTORY
static CFDictionaryRef
verify(const char* tfn, CFDictionaryRef pw)
#else
static struct passwd *
verify(const char *tfn, struct passwd *pw)
#endif
{
#ifdef OPEN_DIRECTORY
	CFMutableDictionaryRef npw;
#else
	struct passwd *npw;
#endif
	ENTRY *ep;
	char *buf, *p, *val;
	struct stat sb;
	FILE *fp;
	int line;
	size_t len;

#ifdef OPEN_DIRECTORY
	if ((npw = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == NULL)
		return (NULL);
#else
	if ((pw = pw_dup(pw)) == NULL)
		return (NULL);
#endif
	if ((fp = fopen(tfn, "r")) == NULL ||
	    fstat(fileno(fp), &sb) == -1) {
		warn("%s", tfn);
#ifndef OPEN_DIRECTORY
		free(pw);
#endif
		return (NULL);
	}
	if (sb.st_size == 0) {
		warnx("corrupted temporary file");
		fclose(fp);
#ifndef OPEN_DIRECTORY
		free(pw);
#endif
		return (NULL);
	}
	val = NULL;
	for (line = 1; (buf = fgetln(fp, &len)) != NULL; ++line) {
		if (*buf == '\0' || *buf == '#')
			continue;
		while (len > 0 && isspace(buf[len - 1]))
			--len;
		for (ep = list;; ++ep) {
			if (!ep->prompt) {
				warnx("%s: unrecognized field on line %d",
				    tfn, line);
				goto bad;
			}
			if (ep->len > len)
				continue;
			if (strncasecmp(buf, ep->prompt, ep->len) != 0)
				continue;
			if (ep->restricted && !master_mode) {
				warnx("%s: you may not change the %s field",
				    tfn, ep->prompt);
				goto bad;
			}
			for (p = buf; p < buf + len && *p != ':'; ++p)
				/* nothing */ ;
			if (*p != ':') {
				warnx("%s: line %d corrupted", tfn, line);
				goto bad;
			}
			while (++p < buf + len && isspace(*p))
				/* nothing */ ;
			free(val);
			asprintf(&val, "%.*s", (int)(buf + len - p), p);
			if (val == NULL)
				goto bad;
			if (ep->except && strpbrk(val, ep->except)) {
				warnx("%s: invalid character in \"%s\" field '%s'",
				    tfn, ep->prompt, val);
				goto bad;
			}
#ifdef OPEN_DIRECTORY
			if ((ep->func)(val, NULL, NULL))
				goto bad;
			{
				CFStringRef str = CFStringCreateWithCString(NULL, val, kCFStringEncodingUTF8);
				if (str) {
					CFDictionarySetValue(npw, ep->attrName, str);
					CFRelease(str);
				}
			}
#else
			if ((ep->func)(val, pw, ep))
				goto bad;
#endif
			break;
		}
	}
	free(val);
	fclose(fp);

#ifndef OPEN_DIRECTORY
	/* Build the gecos field. */
	len = asprintf(&p, "%s,%s,%s,%s,%s", list[E_NAME].save,
	    list[E_LOCATE].save, list[E_BPHONE].save,
	    list[E_HPHONE].save, list[E_OTHER].save);
	if (p == NULL) {
		warn("asprintf()");
		free(pw);
		return (NULL);
	}
	while (len > 0 && p[len - 1] == ',')
		p[--len] = '\0';
	pw->pw_gecos = p;
	buf = pw_make(pw);
	free(pw);
	free(p);
	if (buf == NULL) {
		warn("pw_make()");
		return (NULL);
	}
	npw = pw_scan(buf, PWSCAN_WARN|PWSCAN_MASTER);
#endif /* !OPEN_DIRECTORY */
	free(buf);
	return (npw);
bad:
#ifndef OPEN_DIRECTORY
	free(pw);
#endif
	free(val);
	fclose(fp);
	return (NULL);
}
