/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993, 1994
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chown.c	8.8 (Berkeley) 4/4/94";
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
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <get_compat.h>
#else
#define COMPAT_MODE(a,b) (1)
#endif /* __APPLE__ */

static void	a_gid(const char *);
static void	a_uid(const char *);
static void	chownerr(const char *);
static uid_t	id(const char *, const char *);
static void	usage(void);
static void	print_info(const FTSENT *, int);

static uid_t uid;
static gid_t gid;
static int ischown;
#ifdef __APPLE__
static int isnumeric = 0;
#endif
static const char *gname;
static volatile sig_atomic_t siginfo;

static void
siginfo_handler(int sig __unused)
{

	siginfo = 1;
}

int
main(int argc, char **argv)
{
	FTS *ftsp;
	FTSENT *p;
	int Hflag, Lflag, Pflag, Rflag, fflag, hflag, vflag, xflag;
	int ch, fts_options, rval;
	char *cp;
	int unix2003_compat = 0;

	if (argc < 1)
		usage();
	ischown = (strcmp(basename(argv[0]), "chown") == 0);

	Hflag = Lflag = Pflag = Rflag = fflag = hflag = vflag = xflag = 0;
#ifdef __APPLE__
	while ((ch = getopt(argc, argv, "HLPRfhnvx")) != -1)
#else
	while ((ch = getopt(argc, argv, "HLPRfhvx")) != -1)
#endif
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			hflag = 1;
	 		break;
#ifdef __APPLE__
		case 'n':
			isnumeric = 1;
			break;
#endif
		case 'v':
			vflag++;
			break;
		case 'x':
			xflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();
	if (!Rflag && (Hflag || Lflag || Pflag))
		warnx("options -H, -L, -P only useful with -R");

	(void)signal(SIGINFO, siginfo_handler);

	if (Rflag) {
		if (hflag && (Hflag || Lflag))
			errx(1, "the -R%c and -h options may not be "
			    "specified together", Hflag ? 'H' : 'L');
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

	if (xflag)
		fts_options |= FTS_XDEV;

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	if (ischown) {
		unix2003_compat = COMPAT_MODE("bin/chown", "Unix2003");
		if ((cp = strchr(*argv, ':')) != NULL) {
			*cp++ = '\0';
			a_gid(cp);
		}
#ifdef SUPPORT_DOT
		else if ((cp = strchr(*argv, '.')) != NULL) {
			warnx("separation of user and group with a period is deprecated");
			*cp++ = '\0';
			a_gid(cp);
		}
#endif
		a_uid(*argv);
	} else {
		unix2003_compat = COMPAT_MODE("bin/chgrp", "Unix2003");
		a_gid(*argv);
	}

	if ((ftsp = fts_open(++argv, fts_options, NULL)) == NULL)
		err(1, NULL);

	for (rval = 0; (void)(errno = 0), (p = fts_read(ftsp)) != NULL;) {
		int atflag;

		if ((fts_options & FTS_LOGICAL) ||
		    ((fts_options & FTS_COMFOLLOW) &&
		    p->fts_level == FTS_ROOTLEVEL))
			atflag = 0;
		else
			atflag = AT_SYMLINK_NOFOLLOW;

		switch (p->fts_info) {
		case FTS_D:			/* Change it at FTS_DP. */
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chown. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
#ifdef __APPLE__
		case FTS_SL:
		case FTS_SLNONE:
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything and ones that we found
			 * doing a physical walk.
			 */
			atflag = AT_SYMLINK_NOFOLLOW;
			if (unix2003_compat) {
				if (Hflag || Lflag) {       /* -H or -L was specified */
					if (p->fts_errno) {
						warnx("%s: %s", p->fts_name, strerror(p->fts_errno));
						rval = 1;
						continue;
					}
				}
			}

			break;
#endif
		default:
			break;
		}
		if (siginfo) {
			print_info(p, 2);
			siginfo = 0;
		}
		if (unix2003_compat) {
			/* Can only avoid updating times if both uid and gid are -1 */
			if ((uid == (uid_t)-1) && (gid == (gid_t)-1))
				continue;
		} else {
			if ((uid == (uid_t)-1 || uid == p->fts_statp->st_uid) &&
			    (gid == (gid_t)-1 || gid == p->fts_statp->st_gid))
				continue;
		}
		if (fchownat(AT_FDCWD, p->fts_accpath, uid, gid, atflag)
		    == -1 && !fflag) {
			chownerr(p->fts_path);
			rval = 1;
		} else if (vflag)
			print_info(p, vflag);
	}
	if (errno)
		err(1, "fts_read");
	exit(rval);
}

static void
a_gid(const char *s)
{
	struct group *gr;

	if (*s == '\0')			/* Argument was "uid[:.]". */
		return;
	gname = s;
#ifdef __APPLE__
	gid = (!isnumeric && ((gr = getgrnam(s)) != NULL)) ? gr->gr_gid : id(s, "group");
#else
	gid = ((gr = getgrnam(s)) != NULL) ? gr->gr_gid : id(s, "group");
#endif
}

static void
a_uid(const char *s)
{
	struct passwd *pw;

	if (*s == '\0')			/* Argument was "[:.]gid". */
		return;
#ifdef __APPLE__
	uid = (!isnumeric && ((pw = getpwnam(s)) != NULL)) ? pw->pw_uid : id(s, "user");
#else
	uid = ((pw = getpwnam(s)) != NULL) ? pw->pw_uid : id(s, "user");
#endif
}

static uid_t
id(const char *name, const char *type)
{
	unsigned long val;
	char *ep;

	errno = 0;
	val = strtoul(name, &ep, 10);
	_Static_assert(UID_MAX >= GID_MAX, "UID MAX less than GID MAX");
	if (errno || *ep != '\0' || val > UID_MAX)
		errx(1, "%s: illegal %s name", name, type);
	return (uid_t)val;
}

static void
chownerr(const char *file)
{
	static uid_t euid = -1;
	static int ngroups = -1;
	static long ngroups_max;
	gid_t *groups;

	/* Check for chown without being root. */
	if (errno != EPERM || (uid != (uid_t)-1 &&
	    euid == (uid_t)-1 && (euid = geteuid()) != 0)) {
		warn("%s", file);
		return;
	}

	/* Check group membership; kernel just returns EPERM. */
	if (gid != (gid_t)-1 && ngroups == -1 &&
	    euid == (uid_t)-1 && (euid = geteuid()) != 0) {
		ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;
		if ((groups = malloc(sizeof(gid_t) * ngroups_max)) == NULL)
			err(1, "malloc");
		ngroups = getgroups(ngroups_max, groups);
		while (--ngroups >= 0 && gid != groups[ngroups]);
		free(groups);
		if (ngroups < 0) {
			warnx("you are not a member of group %s", gname);
			return;
		}
	}
	warn("%s", file);
}

static void
usage(void)
{

	if (ischown)
		(void)fprintf(stderr, "%s\n%s\n",
#ifdef __APPLE__
		    "usage: chown [-fhnvx] [-R [-H | -L | -P]] owner[:group]"
		    " file ...",
		    "       chown [-fhnvx] [-R [-H | -L | -P]] :group file ...");
#else
		    "usage: chown [-fhvx] [-R [-H | -L | -P]] owner[:group]"
		    " file ...",
		    "       chown [-fhvx] [-R [-H | -L | -P]] :group file ...");
#endif
	else
		(void)fprintf(stderr, "%s\n",
#ifdef __APPLE__
		    "usage: chgrp [-fhnvx] [-R [-H | -L | -P]] group file ...");
#else
		    "usage: chgrp [-fhvx] [-R [-H | -L | -P]] group file ...");
#endif
	exit(1);
}

static void
print_info(const FTSENT *p, int vflag)
{

	printf("%s", p->fts_path);
	if (vflag > 1) {
		if (ischown) {
			printf(": %ju:%ju -> %ju:%ju",
			    (uintmax_t)p->fts_statp->st_uid, 
			    (uintmax_t)p->fts_statp->st_gid,
			    (uid == (uid_t)-1) ? 
			    (uintmax_t)p->fts_statp->st_uid : (uintmax_t)uid,
			    (gid == (gid_t)-1) ? 
			    (uintmax_t)p->fts_statp->st_gid : (uintmax_t)gid);
		} else {
			printf(": %ju -> %ju", (uintmax_t)p->fts_statp->st_gid,
			    (gid == (gid_t)-1) ? 
			    (uintmax_t)p->fts_statp->st_gid : (uintmax_t)gid);
		}
	}
	printf("\n");
}
