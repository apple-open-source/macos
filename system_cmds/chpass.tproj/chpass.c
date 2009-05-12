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
 * Copyright (c) 1988, 1993, 1994
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chpass.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/chpass/chpass.c,v 1.27.8.1 2006/09/29 06:13:20 marck Exp $");
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef YP
#include <ypclnt.h>
#endif

#ifndef OPEN_DIRECTORY
#include <pw_scan.h>
#include <libutil.h>
#endif

#include "chpass.h"

int master_mode;

#ifdef OPEN_DIRECTORY
#include "open_directory.h"
char *progname = NULL;
CFStringRef DSPath = NULL;
#endif /* OPEN_DIRECTORY */

static void	baduser(void);
static void	usage(void);

int
main(int argc, char *argv[])
{
	enum { NEWSH, LOADENTRY, EDITENTRY, NEWPW, NEWEXP } op;
#ifndef OPEN_DIRECTORY
	struct passwd lpw, *old_pw, *pw;
	int ch, pfd, tfd;
	const char *password;
#else
	struct passwd *old_pw, *pw;
	int ch, tfd;
	char tfn[MAXPATHLEN];
#endif
	char *arg = NULL;
	uid_t uid;
#ifdef YP
	struct ypclnt *ypclnt;
	const char *yp_domain = NULL, *yp_host = NULL;
#endif
#ifdef OPEN_DIRECTORY
	CFStringRef username = NULL;
	CFStringRef authname = NULL;
	CFStringRef location = NULL;
	
	progname = strrchr(argv[0], '/');
	if (progname) progname++;
	else progname = argv[0];
#endif /* OPEN_DIRECTORY */

	pw = old_pw = NULL;
	op = EDITENTRY;
#ifdef OPEN_DIRECTORY
	while ((ch = getopt(argc, argv, "a:s:l:u:")) != -1)
#else /* OPEN_DIRECTORY */
#ifdef YP
	while ((ch = getopt(argc, argv, "a:p:s:e:d:h:loy")) != -1)
#else
	while ((ch = getopt(argc, argv, "a:p:s:e:")) != -1)
#endif
#endif /* OPEN_DIRECTORY */
		switch (ch) {
		case 'a':
			op = LOADENTRY;
			arg = optarg;
			break;
		case 's':
			op = NEWSH;
			arg = optarg;
			break;
#ifndef OPEN_DIRECTORY
		case 'p':
			op = NEWPW;
			arg = optarg;
			break;
		case 'e':
			op = NEWEXP;
			arg = optarg;
			break;
#ifdef YP
		case 'd':
			yp_domain = optarg;
			break;
		case 'h':
			yp_host = optarg;
			break;
		case 'l':
		case 'o':
		case 'y':
			/* compatibility */
			break;
#endif
#else /* OPEN_DIRECTORY */
		case 'l':
			location = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
			break;
		case 'u':
			authname = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
			break;
#endif
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	uid = getuid();

	if (op == EDITENTRY || op == NEWSH || op == NEWPW || op == NEWEXP) {
		if (argc == 0) {
			if ((pw = getpwuid(uid)) == NULL)
				errx(1, "unknown user: uid %lu",
				    (unsigned long)uid);
		} else {
			if ((pw = getpwnam(*argv)) == NULL)
				errx(1, "unknown user: %s", *argv);
#ifndef OPEN_DIRECTORY
			if (uid != 0 && uid != pw->pw_uid)
				baduser();
#endif
		}

#ifndef OPEN_DIRECTORY
		/* Make a copy for later verification */
		if ((pw = pw_dup(pw)) == NULL ||
		    (old_pw = pw_dup(pw)) == NULL)
			err(1, "pw_dup");
#endif
	}

#if OPEN_DIRECTORY
	master_mode = (uid == 0);

	/*
	 * Find the user record and copy its details.
	 */
	username = CFStringCreateWithCString(NULL, pw->pw_name, kCFStringEncodingUTF8);

	if (strcmp(progname, "chsh") == 0 || op == NEWSH) {
		cfprintf(stderr, "Changing shell for %@.\n", username);
	} else if (strcmp(progname, "chfn") == 0) {
		cfprintf(stderr, "Changing finger information for %@.\n", username);
	} else if (strcmp(progname, "chpass") == 0) {
		cfprintf(stderr, "Changing account information for %@.\n", username);
	}
	
	/*
	 * odGetUser updates DSPath global variable, performs authentication
	 * if necessary, and extracts the attributes.
	 */
	CFDictionaryRef attrs_orig = NULL;
	CFDictionaryRef attrs = NULL;
	ODRecordRef rec = odGetUser(location, authname, username, &attrs_orig);
	
	if (!rec || !attrs_orig) exit(1);
#endif /* OPEN_DIRECTORY */

#ifdef YP
	if (pw != NULL && (pw->pw_fields & _PWF_SOURCE) == _PWF_NIS) {
		ypclnt = ypclnt_new(yp_domain, "passwd.byname", yp_host);
		master_mode = (ypclnt != NULL &&
		    ypclnt_connect(ypclnt) != -1 &&
		    ypclnt_havepasswdd(ypclnt) == 1);
		ypclnt_free(ypclnt);
	} else
#endif
	master_mode = (uid == 0);

	if (op == NEWSH) {
		/* protect p_shell -- it thinks NULL is /bin/sh */
		if (!arg[0])
			usage();
		if (p_shell(arg, pw, (ENTRY *)NULL) == -1)
			exit(1);
#ifdef OPEN_DIRECTORY
		else {
			ENTRY* ep;
			
			setrestricted(attrs_orig);
			
			for (ep = list; ep->prompt; ep++) {
				if (strncasecmp(ep->prompt, "shell", ep->len) == 0) {
					if (!ep->restricted) {
						CFStringRef shell = CFStringCreateWithCString(NULL, arg, kCFStringEncodingUTF8);
						if (shell) {
							attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
							if (attrs) CFDictionarySetValue((CFMutableDictionaryRef)attrs, CFSTR(kDS1AttrUserShell), shell);
							CFRelease(shell);
						}
					} else {
						warnx("shell is restricted");
						exit(1);
					}
				}
			}
		}
#endif
	}

#ifndef OPEN_DIRECTORY
	if (op == NEWEXP) {
		if (uid)	/* only root can change expire */
			baduser();
		if (p_expire(arg, pw, (ENTRY *)NULL) == -1)
			exit(1);
	}
#endif

	if (op == LOADENTRY) {
		if (uid)
			baduser();
#ifdef OPEN_DIRECTORY
		warnx("-a is not supported for Open Directory.");
		exit(1);
#else
		pw = &lpw;
		old_pw = NULL;
		if (!__pw_scan(arg, pw, _PWSCAN_WARN|_PWSCAN_MASTER))
			exit(1);
#endif /* OPEN_DIRECTORY */
	}

#ifndef OPEN_DIRECTORY
	if (op == NEWPW) {
		if (uid)
			baduser();

		if (strchr(arg, ':'))
			errx(1, "invalid format for password");
		pw->pw_passwd = arg;
	}
#endif /* OPEN_DIRECTORY */

	if (op == EDITENTRY) {
#ifdef OPEN_DIRECTORY
		setrestricted(attrs_orig);
		snprintf(tfn, sizeof(tfn), "/etc/%s.XXXXXX", progname);
		if ((tfd = mkstemp(tfn)) == -1)
			err(1, "%s", tfn);
		attrs = (CFMutableDictionaryRef)edit(tfn, attrs_orig);
		(void)unlink(tfn);
#else
		/*
		 * We don't really need pw_*() here, but pw_edit() (used
		 * by edit()) is just too useful...
		 */
		if (pw_init(NULL, NULL))
			err(1, "pw_init()");
		if ((tfd = pw_tmp(-1)) == -1) {
			pw_fini();
			err(1, "pw_tmp()");
		}
		free(pw);
		pw = edit(pw_tempname(), old_pw);
		pw_fini();
		if (pw == NULL)
			err(1, "edit()");
		/* 
		 * pw_equal does not check for crypted passwords, so we
		 * should do it explicitly
		 */
		if (pw_equal(old_pw, pw) && 
		    strcmp(old_pw->pw_passwd, pw->pw_passwd) == 0)
			errx(0, "user information unchanged");
#endif /* OPEN_DIRECTORY */
	}

#ifndef OPEN_DIRECTORY
	if (old_pw && !master_mode) {
		password = getpass("Password: ");
		if (strcmp(crypt(password, old_pw->pw_passwd),
		    old_pw->pw_passwd) != 0)
			baduser();
	} else {
		password = "";
	}
#endif

#ifdef OPEN_DIRECTORY
	odUpdateUser(rec, attrs_orig, attrs);
	
	if (rec) CFRelease(rec);

	exit(0);
	return 0;
#else /* OPEN_DIRECTORY */
	exit(0);
	if (old_pw != NULL)
		pw->pw_fields |= (old_pw->pw_fields & _PWF_SOURCE);
	switch (pw->pw_fields & _PWF_SOURCE) {
#ifdef YP
	case _PWF_NIS:
		ypclnt = ypclnt_new(yp_domain, "passwd.byname", yp_host);
		if (ypclnt == NULL ||
		    ypclnt_connect(ypclnt) == -1 ||
		    ypclnt_passwd(ypclnt, pw, password) == -1) {
			warnx("%s", ypclnt->error);
			ypclnt_free(ypclnt);
			exit(1);
		}
		ypclnt_free(ypclnt);
		errx(0, "NIS user information updated");
#endif /* YP */
	case 0:
	case _PWF_FILES:
		if (pw_init(NULL, NULL))
			err(1, "pw_init()");
		if ((pfd = pw_lock()) == -1) {
			pw_fini();
			err(1, "pw_lock()");
		}
		if ((tfd = pw_tmp(-1)) == -1) {
			pw_fini();
			err(1, "pw_tmp()");
		}
		if (pw_copy(pfd, tfd, pw, old_pw) == -1) {
			pw_fini();
			err(1, "pw_copy");
		}
		if (pw_mkdb(pw->pw_name) == -1) {
			pw_fini();
			err(1, "pw_mkdb()");
		}
		pw_fini();
		errx(0, "user information updated");
		break;
	default:
		errx(1, "unsupported passwd source");
	}
#endif /* OPEN_DIRECTORY */
}

static void
baduser(void)
{

	errx(1, "%s", strerror(EACCES));
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: chpass%s %s [user]\n",
#ifdef OPEN_DIRECTORY
		"",
		"[-l location] [-u authname] [-s shell]");
#else /* OPEN_DIRECTORY */
#ifdef YP
	    " [-d domain] [-h host]",
#else
	    "",
#endif
	    "[-a list] [-p encpass] [-s shell] [-e mmm dd yy]");
#endif /* OPEN_DIRECTORY */
	exit(1);
}
