/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
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

#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chmod.c	8.8 (Berkeley) 4/1/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include "chmod_acl.h"

/* Needed by chmod_acl.c. */
int fflag = 0;
#endif /*__APPLE__*/

static volatile sig_atomic_t siginfo;

#ifdef __APPLE__
void usage(void);
#else
static void usage(void);
#endif
static int may_have_nfs4acl(const FTSENT *ent, int hflag);

static void
siginfo_handler(int sig __unused)
{

	siginfo = 1;
}

int
main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *p;
	mode_t *set;
#ifdef __APPLE__
	int Hflag, Lflag, Pflag, Rflag, ch, fts_options, hflag, rval;
#else
	int Hflag, Lflag, Rflag, ch, fflag, fts_options, hflag, rval;
#endif
	int vflag;
	char *ep, *mode;
	mode_t newmode;
#ifdef __APPLE__
	unsigned int acloptflags = 0;
	long aclpos = -1;
	int inheritance_level = 0;
	int index = 0;
	size_t acloptlen = 0;
	int ace_arg_not_required = 0;
	acl_t acl_input = NULL;
#endif /* __APPLE__*/

	ftsp = NULL;
	p = NULL;
	set = NULL;
	Hflag = Lflag = Pflag = Rflag = fflag = hflag = vflag = 0;
#ifndef __APPLE__
	while ((ch = getopt(argc, argv, "HLPRXfghorstuvwx")) != -1)
#else
	while ((ch = getopt(argc, argv, "ACEHILNPRVXafghinorstuvwx")) != -1)
#endif
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			Pflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			Pflag = 1;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			/*
			 * In System V the -h option causes chmod to change
			 * the mode of the symbolic link. 4.4BSD's symbolic
			 * links didn't have modes, so it was an undocumented
			 * noop.  In FreeBSD 3.0, lchmod(2) is introduced and
			 * this option does real work.
			 */
			hflag = 1;
			break;
#ifdef __APPLE__
		case 'a':
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch)
				--optind;
			goto done;
		case 'A':
//			acloptflags |= ACL_FLAG | ACL_TO_STDOUT;
//			ace_arg_not_required = 1;
			errx(1, "-A not implemented");
//			goto done;
		case 'E':
			acloptflags |= ACL_FLAG | ACL_FROM_STDIN;
			goto done;
		case 'C':
			acloptflags |= ACL_FLAG | ACL_CHECK_CANONICITY;
			ace_arg_not_required = 1;
			goto done;
		case 'i':
			acloptflags |= ACL_FLAG | ACL_REMOVE_INHERIT_FLAG;
			ace_arg_not_required = 1;
			goto done;
		case 'I':
			acloptflags |= ACL_FLAG | ACL_REMOVE_INHERITED_ENTRIES;
			ace_arg_not_required = 1;
			goto done;
		case 'n':
			acloptflags |= ACL_FLAG | ACL_NO_TRANSLATE;
			break;
		case 'N':
			acloptflags |= ACL_FLAG | ACL_CLEAR_FLAG;
			ace_arg_not_required = 1;
			goto done;
		case 'V':
//			acloptflags |= ACL_FLAG | ACL_INVOKE_EDITOR;
//			ace_arg_not_required = 1;
			errx(1, "-V not implemented");
//			goto done;
#endif /* __APPLE__ */
		/*
		 * XXX
		 * "-[rwx]" are valid mode commands.  If they are the entire
		 * argument, getopt has moved past them, so decrement optind.
		 * Regardless, we're done argument processing.
		 */
		case 'g': case 'o': case 'r': case 's':
		case 't': case 'u': case 'w': case 'X': case 'x':
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch &&
			    argv[optind - 1][2] == '\0')
				--optind;
			goto done;
		case 'v':
			vflag++;
			break;
		case '?':
		default:
			usage();
		}
done:	argv += optind;
	argc -= optind;

#ifdef __APPLE__
	if (argc < ((acloptflags & ACL_FLAG) ? 1 : 2))
		usage();
	if (!Rflag && (Hflag || Lflag || Pflag))
		warnx("options -H, -L, -P only useful with -R");
#else  /* !__APPLE__ */
	if (argc < 2)
		usage();
#endif	/* __APPLE__ */

	(void)signal(SIGINFO, siginfo_handler);
#ifdef __APPLE__
	if (!(acloptflags & ACL_FLAG) && ((acloptlen = strlen(argv[0])) > 1) && (argv[0][1] == 'a')) {
		acloptflags |= ACL_FLAG;
		switch (argv[0][0]) {
		case '+':
			acloptflags |= ACL_SET_FLAG;
			break;
		case '-':
			acloptflags |= ACL_DELETE_FLAG;
			break;
		case '=':
			acloptflags |= ACL_REWRITE_FLAG;
			break;
		default:
			acloptflags &= ~ACL_FLAG;
			goto apnoacl;
		}
		
		if (argc < 3)
			usage();

		if (acloptlen > 2) {
			for (index = 2; index < acloptlen; index++) {
				switch (argv[0][index]) {
				case '#':
					acloptflags |= ACL_ORDER_FLAG;

					if (argc < ((acloptflags & ACL_DELETE_FLAG)
						    ? 3 : 4))
						usage();
					argv++;
					argc--;
					errno = 0;
					aclpos = strtol(argv[0], &ep, 0);

					if (aclpos > ACL_MAX_ENTRIES
					    || aclpos < 0)
						errno = ERANGE;
					if (errno || *ep)
						errx(1, "Invalid ACL entry number: %ld", aclpos);
					if (acloptflags & ACL_DELETE_FLAG)
						ace_arg_not_required = 1;

					goto apdone;
				case 'i':
					acloptflags |= ACL_INHERIT_FLAG;
					/* The +aii.. syntax to specify
					 * inheritance level is rather unwieldy,
					 * find an alternative.
					 */
					inheritance_level++;
					if (inheritance_level > 1)
						warnx("Inheritance across more than one generation is not currently supported");
					if (inheritance_level >= MAX_INHERITANCE_LEVEL)
						goto apdone;
					break;
				default:
					errno = EINVAL;
					usage();
				}
			}
		}
apdone:
		argv++;
		argc--;
	}
apnoacl:
#endif /*__APPLE__*/

	if (Rflag) {
		if (hflag)
			errx(1, "the -R and -h options may not be "
			    "specified together.");
		if (Lflag) {
			fts_options = FTS_LOGICAL;
		} else {
			fts_options = FTS_PHYSICAL;

			if (Hflag) {
				fts_options |= FTS_COMFOLLOW;
			}
		}
	} else if (hflag) {
		fts_options = FTS_PHYSICAL;
	} else {
		fts_options = FTS_LOGICAL;
	}

#ifdef __APPLE__
	if (acloptflags & ACL_FROM_STDIN) {
		ssize_t readval = 0;
		size_t readtotal = 0;
		
		mode = (char *) malloc(MAX_ACL_TEXT_SIZE);
		
		if (mode == NULL)
			err(1, "Unable to allocate mode string");
		/* Read the ACEs from STDIN */
		do {
			readtotal += readval;
			readval = read(STDIN_FILENO, mode + readtotal, 
				       MAX_ACL_TEXT_SIZE);
		} while ((readval > 0) && (readtotal <= MAX_ACL_TEXT_SIZE));
			
		if (0 == readtotal)
			errx(1, "-E specified, but read from STDIN failed");
		else
			mode[readtotal - 1] = '\0';
		--argv;
	}
	else
#endif /* __APPLE */
		mode = *argv;

#ifdef __APPLE__
	if ((acloptflags & ACL_FLAG)) {

		/* Are we deleting by entry number, verifying
		 * canonicity or performing some other operation that
		 * does not require an input entry? If so, there's no
		 * entry to convert.
		 */
		if (ace_arg_not_required) {
			--argv;
		}
		else {
                        /* Parse the text into an ACL*/
			acl_input = parse_acl_entries(mode);
			if (acl_input == NULL) {
				errx(1, "Invalid ACL specification: %s", mode);
			}
		}
	}
	else {
#endif /* __APPLE__*/
		if ((set = setmode(mode)) == NULL)
			errx(1, "Invalid file mode: %s", mode);
#ifdef __APPLE__
	}
#endif /* __APPLE__*/
	if ((ftsp = fts_open(++argv, fts_options, 0)) == NULL)
		err(1, "fts_open");
	for (rval = 0; (void)(errno = 0), (p = fts_read(ftsp)) != NULL;) {
		int atflag;

		if ((fts_options & FTS_LOGICAL) ||
		    ((fts_options & FTS_COMFOLLOW) &&
		    p->fts_level == FTS_ROOTLEVEL))
			atflag = 0;
		else
			atflag = AT_SYMLINK_NOFOLLOW;

		switch (p->fts_info) {
		case FTS_D:
			if (!Rflag)
				(void)fts_set(ftsp, p, FTS_SKIP);
			break;
		case FTS_DNR:			/* Warn, chmod. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_DP:			/* Already changed at FTS_D. */
			continue;
#ifdef __APPLE__
		case FTS_NS:
			if (acloptflags & ACL_FLAG) /* don't need stat for -N */
				break;
#endif
		case FTS_ERR:			/* Warn, continue. */
#ifndef __APPLE__
		case FTS_NS:
#endif
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		default:
			break;
		}
#ifdef __APPLE__
		/* If an ACL manipulation option was specified, manipulate */
		if (acloptflags & ACL_FLAG)	{
			if (0 != modify_file_acl(acloptflags, p->fts_accpath, acl_input, (int)aclpos, inheritance_level, !hflag))
				rval = 1;
		}
		else {
#endif /* __APPLE__ */
		newmode = getmode(set, p->fts_statp->st_mode);
		/*
		 * With NFSv4 ACLs, it is possible that applying a mode
		 * identical to the one computed from an ACL will change
		 * that ACL.
		 */
		if (may_have_nfs4acl(p, hflag) == 0 &&
		    (newmode & ALLPERMS) == (p->fts_statp->st_mode & ALLPERMS))
				continue;
		if (fchmodat(AT_FDCWD, p->fts_accpath, newmode, atflag) == -1
		    && !fflag) {
#ifdef __APPLE__
			warn("Unable to change file mode on %s", p->fts_path);
#else
			warn("%s", p->fts_path);
#endif
			rval = 1;
		} else if (vflag || siginfo) {
			(void)printf("%s", p->fts_path);

			if (vflag > 1 || siginfo) {
				char m1[12], m2[12];

				strmode(p->fts_statp->st_mode, m1);
				strmode((p->fts_statp->st_mode &
				    S_IFMT) | newmode, m2);
				(void)printf(": 0%o [%s] -> 0%o [%s]",
				    p->fts_statp->st_mode, m1,
				    (p->fts_statp->st_mode & S_IFMT) |
				    newmode, m2);
			}
			(void)printf("\n");
			siginfo = 0;
		}
#ifdef __APPLE__
		}
#endif /* __APPLE__*/
	}
	if (errno)
		err(1, "fts_read");
#ifdef __APPLE__
	if (acl_input)
		acl_free(acl_input);
	if (mode && (acloptflags & ACL_FROM_STDIN))
		free(mode);
	
#endif /* __APPLE__ */
	exit(rval);
}

#ifdef __APPLE__
void
#else
static void
#endif
usage(void)
{
#ifdef __APPLE__
	(void)fprintf(stderr,
		      "usage:\tchmod [-fhv] [-R [-H | -L | -P]] [-a | +a | =a  [i][# [ n]]] mode|entry file ...\n"
		      "\tchmod [-fhv] [-R [-H | -L | -P]] [-E | -C | -N | -i | -I] file ...\n"); /* add -A and -V when implemented */
#else
	(void)fprintf(stderr,
	    "usage: chmod [-fhv] [-R [-H | -L | -P]] mode file ...\n");
#endif /* __APPLE__ */
	exit(1);
}

static int
may_have_nfs4acl(const FTSENT *ent, int hflag)
{
#ifdef __APPLE__
	return (0);
#else
	int ret;
	static dev_t previous_dev = NODEV;
	static int supports_acls = -1;

	if (previous_dev != ent->fts_statp->st_dev) {
		previous_dev = ent->fts_statp->st_dev;
		supports_acls = 0;

		if (hflag)
			ret = lpathconf(ent->fts_accpath, _PC_ACL_NFS4);
		else
			ret = pathconf(ent->fts_accpath, _PC_ACL_NFS4);
		if (ret > 0)
			supports_acls = 1;
		else if (ret < 0 && errno != EINVAL)
			warn("%s", ent->fts_path);
	}

	return (supports_acls);
#endif
}
