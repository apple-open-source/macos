/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*	$NetBSD: mount_msdos.c,v 1.18 1997/09/16 12:24:18 lukem Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
 * All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sbin/mount_msdos/mount_msdos.c,v 1.19 2000/01/08 16:47:55 ache Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include "../msdosfs.kextproj/msdosfs.kmodproj/msdosfsmount.h"

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
/* must be after stdio to declare fparseln */
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
//#include <util.h>

/* bek 5/20/98 - [2238317] - mntopts.h needs to be installed in a public place */

#define Radar_2238317 1

#if ! Radar_2238317

#include <mntopts.h>

#else //  Radar_2238317

/*-
 * Copyright (c) 1994
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
 *
 *	@(#)mntopts.h	8.7 (Berkeley) 3/29/95
 */

struct mntopt {
    const char *m_option;	/* option name */
    int m_inverse;		/* if a negative option, eg "dev" */
    int m_flag;		/* bit to set, eg. MNT_RDONLY */
    int m_altloc;		/* 1 => set bit in altflags */
};

/* User-visible MNT_ flags. */
#define MOPT_ASYNC		{ "async",	0, MNT_ASYNC, 0 }
#define MOPT_NODEV		{ "dev",	1, MNT_NODEV, 0 }
#define MOPT_NOEXEC		{ "exec",	1, MNT_NOEXEC, 0 }
#define MOPT_NOSUID		{ "suid",	1, MNT_NOSUID, 0 }
#define MOPT_RDONLY		{ "rdonly",	0, MNT_RDONLY, 0 }
#define MOPT_SYNC		{ "sync",	0, MNT_SYNCHRONOUS, 0 }
#define MOPT_UNION		{ "union",	0, MNT_UNION, 0 }
#define MOPT_USERQUOTA		{ "userquota",	0, 0, 0 }
#define MOPT_GROUPQUOTA		{ "groupquota",	0, 0, 0 }
#define MOPT_PERMISSIONS	{ "perm", 1, MNT_UNKNOWNPERMISSIONS, 0 }

/* Control flags. */
#define MOPT_FORCE		{ "force",	0, MNT_FORCE, 0 }
#define MOPT_UPDATE		{ "update",	0, MNT_UPDATE, 0 }
#define MOPT_RO			{ "ro",		0, MNT_RDONLY, 0 }
#define MOPT_RW			{ "rw",		1, MNT_RDONLY, 0 }

/* This is parsed by mount(8), but is ignored by specific mount_*(8)s. */
#define MOPT_AUTO		{ "auto",	0, 0, 0 }

#define MOPT_FSTAB_COMPAT						\
    MOPT_RO,							\
    MOPT_RW,							\
    MOPT_AUTO

/* Standard options which all mounts can understand. */
#define MOPT_STDOPTS							\
	MOPT_USERQUOTA,							\
	MOPT_GROUPQUOTA,						\
	MOPT_FSTAB_COMPAT,						\
	MOPT_NODEV,							\
	MOPT_NOEXEC,							\
	MOPT_NOSUID,							\
	MOPT_RDONLY,							\
	MOPT_UNION,								\
	MOPT_PERMISSIONS

void getmntopts __P((const char *, const struct mntopt *, int *, int *));
void checkpath __P((const char *, char resolved_path[]));
void rmslashes __P((char *, char *));
extern int getmnt_silent;

#endif // Radar_2238317

/*
 * XXX - no way to specify "foo=<bar>"-type options; that's what we'd
 * want for "-u", "-g", "-m", "-L", and "-W".
 */
static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_SYNC,
	MOPT_UPDATE,
#ifdef MSDOSFSMNT_GEMDOSFS
	{ "gemdosfs", 0, MSDOSFSMNT_GEMDOSFS, 1 },
#endif
	{ "shortnames", 0, MSDOSFSMNT_SHORTNAME, 1 },
	{ "longnames", 0, MSDOSFSMNT_LONGNAME, 1 },
	{ "nowin95", 0, MSDOSFSMNT_NOWIN95, 1 },
	{ NULL }
};

static gid_t	a_gid __P((char *));
static uid_t	a_uid __P((char *));
static mode_t	a_mask __P((char *));
static void		usage __P((void));
#if 0
static void     load_u2wtable __P((struct msdosfs_args *, char *));
static void     load_ultable __P((struct msdosfs_args *, char *));
#endif

static int checkLoadable();
static char *progname;
static int load_kmod();

#define DEBUG 0
#if DEBUG
#define dprintf(x) printf x;
#else
#define dprintf(x) ;
#endif

int
main(argc, argv)
	int argc;
	char **argv;
{
        struct msdosfs_args args;
        struct stat sb;
        int c, mntflags, set_gid, set_uid, set_mask;
        //int error;
        char *dev, *dir, mntpath[MAXPATHLEN];
        // struct vfsconf vfc;

	mntflags = set_gid = set_uid = set_mask = 0;
	(void)memset(&args, '\0', sizeof(args));
	args.magic = MSDOSFS_ARGSMAGIC;
        progname = argv[0];

	while ((c = getopt(argc, argv, "sl9u:g:m:o:")) != -1) {
		switch (c) {
#ifdef MSDOSFSMNT_GEMDOSFS
		case 'G':
			args.flags |= MSDOSFSMNT_GEMDOSFS;
			break;
#endif
		case 's':
			args.flags |= MSDOSFSMNT_SHORTNAME;
			break;
		case 'l':
			args.flags |= MSDOSFSMNT_LONGNAME;
			break;
		case '9':
			args.flags |= MSDOSFSMNT_NOWIN95;
			break;
		case 'u':
			args.uid = a_uid(optarg);
			set_uid = 1;
			break;
		case 'g':
			args.gid = a_gid(optarg);
			set_gid = 1;
			break;
		case 'm':
			args.mask = a_mask(optarg);
			set_mask = 1;
			break;
#if 0
		case 'L':
			load_ultable(&args, optarg);
			args.flags |= MSDOSFSMNT_ULTABLE;
			break;
		case 'W':
			load_u2wtable(&args, optarg);
			args.flags |= MSDOSFSMNT_U2WTABLE;
			break;
#endif
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &args.flags);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

	if (optind + 2 != argc)
		usage();

	dev = argv[optind];
	dir = argv[optind + 1];

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	(void)checkpath(dir, mntpath);
	(void)rmslashes(dev, dev);

	args.fspec = dev;
	args.export.ex_root = -2;	/* unchecked anyway on DOS fs */
	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;
	if (!set_gid || !set_uid || !set_mask) {
		if (stat(mntpath, &sb) == -1)
			err(EX_OSERR, "stat %s", mntpath);

		if (!set_uid)
			args.uid = sb.st_uid;
		if (!set_gid)
			args.gid = sb.st_gid;
		if (!set_mask)
			args.mask = sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
		
		/* If NO permisions are given, then set the whole mount to unknown */
		if (!set_gid && !set_uid && !set_mask) {
			args.mask = ACCESSPERMS;
			mntflags |= MNT_UNKNOWNPERMISSIONS;
			}
	}

#if 0
	error = getvfsbyname("msdos", &vfc);
	if (error && vfsisloadable("msdos")) {
		if (vfsload("msdos"))
			err(EX_OSERR, "vfsload(msdos)");
		endvfsent();	/* clear cache */
		error = getvfsbyname("msdos", &vfc);
	}
	if (error)
		errx(EX_OSERR, "msdos filesystem is not available");
#endif

	if (checkLoadable())		/* Is it already loaded? */
                if (load_kmod())		/* Load it in */
			errx(EX_OSERR, "msdos filesystem is not available");

	if (mount("msdos", mntpath, mntflags, &args) < 0)
		err(EX_OSERR, "%s on %s", args.fspec, mntpath);

	exit (0);
}

gid_t
a_gid(s)
	char *s;
{
	struct group *gr;
	char *gname;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		gid = gr->gr_gid;
	else {
		for (gname = s; *s && isdigit(*s); ++s);
		if (!*s)
			gid = atoi(gname);
		else
			errx(EX_NOUSER, "unknown group id: %s", gname);
	}
	return (gid);
}

uid_t
a_uid(s)
	char *s;
{
	struct passwd *pw;
	char *uname;
	uid_t uid;

	if ((pw = getpwnam(s)) != NULL)
		uid = pw->pw_uid;
	else {
		for (uname = s; *s && isdigit(*s); ++s);
		if (!*s)
			uid = atoi(uname);
		else
			errx(EX_NOUSER, "unknown user id: %s", uname);
	}
	return (uid);
}

mode_t
a_mask(s)
	char *s;
{
	int done, rv;
	char *ep;

	done = 0;
	rv = -1;
	if (*s >= '0' && *s <= '7') {
		done = 1;
		rv = strtol(optarg, &ep, 8);
	}
	if (!done || rv < 0 || *ep)
		errx(EX_USAGE, "invalid file mode: %s", s);
	return (rv);
}

void
usage()
{
	fprintf(stderr, "%s\n%s\n", 
	"usage: mount_msdos [-o options] [-u user] [-g group] [-m mask]",
	"                   [-s] [-l] [-9] bdev dir");
	exit(EX_USAGE);
}


#define FS_TYPE			"msdos"

static int checkLoadable()
{
        struct vfsconf vfc;
        int name[4], maxtypenum, cnt;
        size_t buflen;

        name[0] = CTL_VFS;
        name[1] = VFS_GENERIC;
        name[2] = VFS_MAXTYPENUM;
        buflen = 4;
        if (sysctl(name, 3, &maxtypenum, &buflen, (void *)0, (size_t)0) < 0)
                return (-1);
        name[2] = VFS_CONF;
        buflen = sizeof vfc;
        for (cnt = 0; cnt < maxtypenum; cnt++) {
                name[3] = cnt;
                if (sysctl(name, 4, &vfc, &buflen, (void *)0, (size_t)0) < 0) {
                        if (errno != EOPNOTSUPP && errno != ENOENT)
                                return (-1);
                        continue;
                }
                if (!strcmp(FS_TYPE, vfc.vfc_name))
                        return (0);
        }
        errno = ENOENT;
        return (-1);

}

#define LOAD_COMMAND "/sbin/kextload"
#define MSDOS_MODULE_PATH "/System/Library/Extensions/msdosfs.kext"


static int load_kmod()
{

        int pid;
        int result = -1;
        union wait status;

        pid = fork();
        if (pid == 0) {
                result = execl(LOAD_COMMAND, LOAD_COMMAND, MSDOS_MODULE_PATH,NULL);
                /* We can only get here if the exec failed */
                goto Err_Exit;
        }

        if (pid == -1) {
                result = errno;
                goto Err_Exit;
        }

        /* Success! */
        if ((wait4(pid, (int *)&status, 0, NULL) == pid) && (WIFEXITED(status))) {
                result = status.w_retcode;
        }
        else {
                result = -1;
        }


Err_Exit:

                return (result);
}


#if 0

void
load_u2wtable (pargs, name)
	struct msdosfs_args *pargs;
	char *name;
{
	FILE *f;
	int i, j, code[8];
	size_t line = 0;
	char buf[128];
	char *fn, *s, *p;

	if (*name == '/')
		fn = name;
	else {
		snprintf(buf, sizeof(buf), "/usr/share/msdosfs/%s", name);
		buf[127] = '\0';
		fn = buf;
	}
	if ((f = fopen(fn, "r")) == NULL)
		err(EX_NOINPUT, "%s", fn);
	p = NULL;
	for (i = 0; i < 16; i++) {
		do {
			if (p != NULL) free(p);
			if ((p = s = fparseln(f, NULL, &line, NULL, 0)) == NULL)
				errx(EX_DATAERR, "can't read u2w table row %d near line %d", i, line);
			while (isspace((unsigned char)*s))
				s++;
		} while (*s == '\0');
		if (sscanf(s, "%i%i%i%i%i%i%i%i",
code, code + 1, code + 2, code + 3, code + 4, code + 5, code + 6, code + 7) != 8)
			errx(EX_DATAERR, "u2w table: missing item(s) in row %d, line %d", i, line);
		for (j = 0; j < 8; j++)
			pargs->u2w[i * 8 + j] = code[j];
	}
	for (i = 0; i < 16; i++) {
		do {
			free(p);
			if ((p = s = fparseln(f, NULL, &line, NULL, 0)) == NULL)
				errx(EX_DATAERR, "can't read d2u table row %d near line %d", i, line);
			while (isspace((unsigned char)*s))
				s++;
		} while (*s == '\0');
		if (sscanf(s, "%i%i%i%i%i%i%i%i",
code, code + 1, code + 2, code + 3, code + 4, code + 5, code + 6, code + 7) != 8)
			errx(EX_DATAERR, "d2u table: missing item(s) in row %d, line %d", i, line);
		for (j = 0; j < 8; j++)
			pargs->d2u[i * 8 + j] = code[j];
	}
	for (i = 0; i < 16; i++) {
		do {
			free(p);
			if ((p = s = fparseln(f, NULL, &line, NULL, 0)) == NULL)
				errx(EX_DATAERR, "can't read u2d table row %d near line %d", i, line);
			while (isspace((unsigned char)*s))
				s++;
		} while (*s == '\0');
		if (sscanf(s, "%i%i%i%i%i%i%i%i",
code, code + 1, code + 2, code + 3, code + 4, code + 5, code + 6, code + 7) != 8)
			errx(EX_DATAERR, "u2d table: missing item(s) in row %d, line %d", i, line);
		for (j = 0; j < 8; j++)
			pargs->u2d[i * 8 + j] = code[j];
	}
	free(p);
	fclose(f);
}

void
load_ultable (pargs, name)
	struct msdosfs_args *pargs;
	char *name;
{
	int i;

	if (setlocale(LC_CTYPE, name) == NULL)
		err(EX_CONFIG, name);
	for (i = 0; i < 128; i++) {
		pargs->ul[i] = tolower(i | 0x80);
		pargs->lu[i] = toupper(i | 0x80);
	}
}

#endif
