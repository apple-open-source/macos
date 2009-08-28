/*
 * Copyright (c) 1999-2007 Apple Inc. All rights reserved.
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
 * Copyright (c) 1980, 1989, 1993, 1994
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
#include <sys/mount.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

int debug, verbose;

int	checkvfsname __P((const char *, const char **));
char   *catopt __P((char *, const char *));
struct statfs64 *getmntpt __P((const char *));
int	hasopt __P((const char *, const char *));
int	ismounted __P((const char *, const char *));
const char
      **makevfslist __P((char *));
void	mangle __P((char *, int *, const char **));
int	mountfs __P((const char *, const char *, const char *,
			int, const char *, const char *));
void	prmount __P((struct statfs64 *));
void	usage __P((void));

/* From mount_ufs.c. */
int	mount_ufs __P((int, char * const *));

/* Map from mount otions to printable formats. */
static struct opt {
	int o_opt;
	const char *o_name;
} optnames[] = {
	{ MNT_ASYNC,		"asynchronous" },
	{ MNT_EXPORTED,		"NFS exported" },
	{ MNT_LOCAL,		"local" },
	{ MNT_NODEV,		"nodev" },
	{ MNT_NOEXEC,		"noexec" },
	{ MNT_NOSUID,		"nosuid" },
	{ MNT_QUOTA,		"with quotas" },
	{ MNT_RDONLY,		"read-only" },
	{ MNT_SYNCHRONOUS,	"synchronous" },
	{ MNT_UNION,		"union" },
	{ MNT_AUTOMOUNTED,	"automounted" },
	{ MNT_JOURNALED,	"journaled" },
	{ MNT_DEFWRITE, 	"defwrite" },
	{ MNT_IGNORE_OWNERSHIP,	"noowners" },
	{ MNT_NOATIME,		"noatime" },
	{ MNT_QUARANTINE,	"quarantine" },
	{ MNT_DONTBROWSE,	"nobrowse"},
	{ 0, 				NULL }
};

int
main(argc, argv)
	int argc;
	char * const argv[];
{
	const char *mntfromname, **vfslist, *vfstype;
	struct fstab *fs;
	struct statfs64 *mntbuf;
	int all, ch, i, init_flags, mntsize, rval;
	char *options;

	all = init_flags = 0;
	options = NULL;
	vfslist = NULL;
	vfstype = "ufs";
	while ((ch = getopt(argc, argv, "adfo:rwt:uv")) != EOF)
		switch (ch) {
		case 'a':
			all = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			init_flags |= MNT_FORCE;
			break;
		case 'o':
			if (*optarg)
				options = catopt(options, optarg);
			break;
		case 'r':
			init_flags |= MNT_RDONLY;
			break;
		case 't':
			if (vfslist != NULL)
				errx(1, "only one -t option may be specified.");
			vfslist = makevfslist(optarg);
			vfstype = optarg;
			break;
		case 'u':
			init_flags |= MNT_UPDATE;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			init_flags &= ~MNT_RDONLY;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

#define	BADTYPE(type)							\
	(strcmp(type, FSTAB_RO) &&					\
	    strcmp(type, FSTAB_RW) && strcmp(type, FSTAB_RQ))

	rval = 0;
	switch (argc) {
	case 0:
		if (all) {
			setfsent();
			while ((fs = getfsent()) != NULL) {
				if (BADTYPE(fs->fs_type))
					continue;
				if (checkvfsname(fs->fs_vfstype, vfslist))
					continue;
				if (hasopt(fs->fs_mntops, "noauto"))
					continue;
				if (!strcmp(fs->fs_vfstype, "nfs")) {
					if (hasopt(fs->fs_mntops, "net"))
						continue;
					/* check if already mounted */
					if (fs->fs_spec == NULL || 
					    fs->fs_file == NULL ||
					    ismounted(fs->fs_spec, fs->fs_file))
						continue;
				}
				if (mountfs(fs->fs_vfstype, fs->fs_spec,
				    fs->fs_file, init_flags, options,
				    fs->fs_mntops))
					rval = 1;
			}
			endfsent();
        	} else {
			if ((mntsize = getmntinfo64(&mntbuf, MNT_NOWAIT)) == 0)
				err(1, "getmntinfo64");
			for (i = 0; i < mntsize; i++) {
				if (checkvfsname(mntbuf[i].f_fstypename, vfslist))
					continue;
				prmount(&mntbuf[i]);
			}
		}
		exit(rval);
	case 1:
		if (vfslist != NULL)
			usage();

		if (init_flags & MNT_UPDATE) {
			if ((mntbuf = getmntpt(*argv)) == NULL)
				errx(1,
				    "unknown special file or file system %s.",
				    *argv);
			/*
			 * Handle the special case of upgrading the root file
			 * system from read-only to read/write.  The root file
			 * system was originally mounted with a "mount from" name
			 * of "root_device".  The getfsfile("/") returns non-
			 * NULL at this point, with fs_spec set to the true
			 * path to the root device (regardless of what /etc/fstab
			 * contains).
			 */
			mntfromname = mntbuf->f_mntfromname;
			if (strchr(mntfromname, '/') == NULL) {
				fs = getfsfile(mntbuf->f_mntonname);
				if (fs != NULL)
					mntfromname = fs->fs_spec;
			}
			
			/* Do the update mount */
			rval = mountfs(mntbuf->f_fstypename, mntfromname,
			    mntbuf->f_mntonname, init_flags, options, 0);
			break;
		}
		if ((fs = getfsfile(*argv)) == NULL &&
		    (fs = getfsspec(*argv)) == NULL)
			errx(1, "%s: unknown special file or file system.",
			    *argv);
		if (BADTYPE(fs->fs_type))
			errx(1, "%s has unknown file system type.",
			    *argv);
		if (!strcmp(fs->fs_vfstype, "nfs")) {
			if (hasopt(fs->fs_mntops, "net"))
				errx(1, "%s is owned by the automounter.", *argv);
			if (ismounted(fs->fs_spec, fs->fs_file))
				errx(1, "%s is already mounted at %s.",
					fs->fs_spec, fs->fs_file);
		}
		rval = mountfs(fs->fs_vfstype, fs->fs_spec, fs->fs_file,
		    init_flags, options, fs->fs_mntops);
		break;
	case 2:
		/*
		 * If -t flag has not been specified, and spec contains either
		 * a ':' or a '@' then assume that an NFS filesystem is being
		 * specified ala Sun.
		 */
		if (vfslist == NULL && strpbrk(argv[0], ":@") != NULL) {
			vfstype = "nfs";
			/* check if already mounted */
			if (ismounted(argv[0], argv[1]))
				errx(1, "%s is already mounted at %s.",
					argv[0], argv[1]);
		}

		/* If we have both a devnode and a pathname, and an update mount was requested,
		 * then figure out where the devnode is mounted.  We will need to run 
		 * an update mount on its path.  It wouldn't make sense to do an
		 * update mount on a path other than the one it's already using.
		 *
		 * XXX: Should we implement the same workaround for updating the
		 * root file system at boot time?
		 */
		if (init_flags & MNT_UPDATE) {
			if ((mntbuf = getmntpt(*argv)) == NULL)
				errx(1,"unknown special file or file system %s.", *argv);
			rval = mountfs(mntbuf->f_fstypename, mntbuf->f_mntfromname,
					mntbuf->f_mntonname, init_flags, options, 0);
		}
		else {
			/* If update mount not requested, then go with default vfstype and arguments */
			rval = mountfs(vfstype,
					argv[0], argv[1], init_flags, options, NULL);
		}
		break;
	default:
		usage();
		/* NOTREACHED */
	}

	exit(rval);
}

int
hasopt(mntopts, option)
	const char *mntopts, *option;
{
	int negative, found;
	char *opt, *optbuf;

	if (option[0] == 'n' && option[1] == 'o') {
		negative = 1;
		option += 2;
	} else
		negative = 0;
	optbuf = strdup(mntopts);
	found = 0;
	for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		if (opt[0] == 'n' && opt[1] == 'o') {
			if (!strcasecmp(opt + 2, option))
				found = negative;
		} else if (!strcasecmp(opt, option))
			found = !negative;
	}
	free(optbuf);
	return (found);
}

int
ismounted(fs_spec, fs_file)
	const char *fs_spec, *fs_file;
{
	int i, mntsize;
	struct statfs64 *mntbuf;

	if ((mntsize = getmntinfo64(&mntbuf, MNT_NOWAIT)) == 0)
		err(1, "getmntinfo64");
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mntbuf[i].f_mntfromname, fs_spec))
			continue;
		if (strcmp(mntbuf[i].f_mntonname, fs_file))
			continue;
		return 1;
	}
	return 0;
}

int
mountfs(vfstype, spec, name, flags, options, mntopts)
	const char *vfstype, *spec, *name, *options, *mntopts;
	int flags;
{
	/* List of directories containing mount_xxx subcommands. */
	static const char *edirs[] = {
		_PATH_SBIN,
		_PATH_USRSBIN,
		_PATH_FSBNDL,	/* always last */
		NULL
	};
	const char *argv[100], **edir;
	struct statfs64 sf;
	pid_t pid;
	int argc, i, status;
	char *optbuf, execname[MAXPATHLEN + 1], mntpath[MAXPATHLEN];

	if (realpath(name, mntpath) == NULL) {
		warn("realpath %s", mntpath);
		return (1);
	}

	name = mntpath;

	if (mntopts == NULL)
		mntopts = "";
	if (options == NULL) {
		if (*mntopts == '\0') {
			options = "";
		} else {
			options = mntopts;
			mntopts = "";
		}
	}
	optbuf = catopt(strdup(mntopts), options);

	if (strcmp(name, "/") == 0)
		flags |= MNT_UPDATE;
	if (flags & MNT_FORCE)
		optbuf = catopt(optbuf, "force");
	if (flags & MNT_RDONLY)
		optbuf = catopt(optbuf, "ro");
	if (flags & MNT_UPDATE)
		optbuf = catopt(optbuf, "update");

	argc = 0;
	argv[argc++] = vfstype;
	mangle(optbuf, &argc, argv);
	argv[argc++] = spec;
	argv[argc++] = name;
	argv[argc] = NULL;

	if (debug) {
		(void)printf("exec: mount_%s", vfstype);
		for (i = 1; i < argc; i++)
			(void)printf(" %s", argv[i]);
		(void)printf("\n");
		return (0);
	}

	switch (pid = fork()) {
	case -1:				/* Error. */
		warn("fork");
		free(optbuf);
		return (1);
	case 0:					/* Child. */
		if (strcmp(vfstype, "ufs") == 0)
			exit(mount_ufs(argc, (char * const *) argv));

		/* Go find an executable. */
		edir = edirs;
		do {
			/* Special case file system bundle executable path */
			if (*(edir+1) == NULL)
				(void)snprintf(execname, sizeof(execname),
					"%s/%s.fs/%s/mount_%s", *edir,
					vfstype, _PATH_FSBNDLBIN, vfstype);
			else
				(void)snprintf(execname, sizeof(execname),
					"%s/mount_%s", *edir, vfstype);

			argv[0] = execname;
			execv(execname, (char * const *)argv);
			if (errno != ENOENT)
				warn("exec %s for %s", execname, name);
		} while (*++edir != NULL);

		if (errno == ENOENT)
			warn("exec %s for %s", execname, name);
		exit(1);
		/* NOTREACHED */
	default:				/* Parent. */
		free(optbuf);

		if (waitpid(pid, &status, 0) < 0) {
			warn("waitpid");
			return (1);
		}

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				return (WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			warnx("%s: %s", name, sys_siglist[WTERMSIG(status)]);
			return (1);
		}

		if (verbose) {
			if (statfs64(name, &sf) < 0) {
				warn("statfs64 %s", name);
				return (1);
			}
			prmount(&sf);
		}
		break;
	}

	return (0);
}

void
prmount(sfp)
	struct statfs64 *sfp;
{
	int flags;
	struct opt *o;
	struct passwd *pw;

	(void)printf("%s on %s (%s", sfp->f_mntfromname, sfp->f_mntonname,
	    sfp->f_fstypename);

	flags = sfp->f_flags & MNT_VISFLAGMASK;
	for (o = optnames; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			(void)printf(", %s", o->o_name);
			flags &= ~o->o_opt;
		}
	if (sfp->f_owner) {
		(void)printf(", mounted by ");
		if ((pw = getpwuid(sfp->f_owner)) != NULL)
			(void)printf("%s", pw->pw_name);
		else
			(void)printf("%d", sfp->f_owner);
	}
	(void)printf(")\n");
}

struct statfs64 *
getmntpt(name)
	const char *name;
{
	struct statfs64 *mntbuf;
	int i, mntsize;

	mntsize = getmntinfo64(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mntbuf[i].f_mntfromname, name) == 0 ||
		    strcmp(mntbuf[i].f_mntonname, name) == 0)
			return (&mntbuf[i]);
	}
	return (NULL);
}

char *
catopt(s0, s1)
	char *s0;
	const char *s1;
{
	size_t i;
	char *cp;

	if (s0 && *s0) {
		i = strlen(s0) + strlen(s1) + 1 + 1;
		if ((cp = malloc(i)) == NULL)
			err(1, NULL);
		(void)snprintf(cp, i, "%s,%s", s0, s1);
	} else
		cp = strdup(s1);

	if (s0)
		free(s0);
	return (cp);
}

void
mangle(options, argcp, argv)
	char *options;
	int *argcp;
	const char **argv;
{
	char *p, *s;
	int argc;

	argc = *argcp;
	for (s = options; (p = strsep(&s, ",")) != NULL;)
		if (*p != '\0')
			if (*p == '-') {
				argv[argc++] = p;
				p = strchr(p, '=');
				if (p) {
					*p = '\0';
					argv[argc++] = p+1;
				}
			} else {
				argv[argc++] = "-o";
				argv[argc++] = p;
			}

	*argcp = argc;
}

void
usage()
{

	(void)fprintf(stderr,
		"usage: mount %s %s\n       mount %s\n       mount %s\n",
		"[-dfruvw] [-o options] [-t ufs | external_type]",
			"special node",
		"[-adfruvw] [-t ufs | external_type]",
		"[-dfruvw] special | node");
	exit(1);
}
