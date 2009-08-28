/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1993, 1994
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

/********
static char copyright[] =
"@(#) Copyright (c) 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
********/

/********
static char sccsid[] = "@(#)mount_ufs.c	8.4 (Berkeley) 4/26/95";
********/

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/appleapiopts.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ufs_args {
	char *fspec; // block special device to mount
};

#include <mntopts.h>

void	ufs_usage __P((void));

static int checkLoadable(void);
static int load_kmod(void);

#define FS_TYPE "ufs"
#define LOAD_COMMAND "/sbin/kextload"
#define UFS_MODULE_PATH "/System/Library/Extensions/ufs.kext"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_ASYNC,
	MOPT_SYNC,
	MOPT_UPDATE,
	MOPT_FORCE,
	{ NULL }
};

static int checkLoadable(void) {
	int error;
	struct vfsconf vfc;
	
	error = getvfsbyname(FS_TYPE, &vfc);
	
	return error;
}

static int load_kmod(void) {
	int pid;
	int result = -1;
	union wait status;
	
	pid = fork();
	if (pid == 0) {
		/* in the child process */
		result = execl(LOAD_COMMAND, LOAD_COMMAND, UFS_MODULE_PATH, NULL);
		/* we'll only return if execl failed */
		return -1;	
	}

	if (pid == -1) {
		/* fork failed */
		result = errno;
		return result;
	}
	/* otherwise, wait for the kextload to finish */
	if ((wait4(pid, (int*)&status, 0, NULL) == pid) && (WIFEXITED(status))) {
		result = status.w_retcode;
	}
	else {
		result = -1;
	}
	
	return result;
}	



int
mount_ufs(argc, argv)
	int argc;
	char * const argv[];
{
	extern int optreset;
	struct ufs_args args;
	struct statfs fsinfo;
	int ch, mntflags, noasync;
	char *fs_name;
	mntoptparse_t tmp;

	mntflags = 0;
	noasync = 0;
	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "o:")) != EOF)
		switch (ch) {
		case 'o':
			if (strstr(optarg, "noasync") != NULL)
				noasync = 1;
			tmp = getmntopts(optarg, mopts, &mntflags, 0);
			if (mntflags & MNT_SYNCHRONOUS)
				noasync = 1;
			freemntopts(tmp);
			break;
		case '?':
		default:
			ufs_usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		ufs_usage();

        args.fspec = argv[0];		/* The name of the device file. */
	fs_name = argv[1];		/* The mount point. */

	/*
	 * In the case of a mount point update, the mount system call below
	 * will used, and it will succeed even if the volume is not UFS. Thus,
	 * the noasync flag should be forced unless the volume actually is UFS.
	 */
	if (statfs(fs_name, &fsinfo) == 0) {
		if (strncmp(fsinfo.f_mntonname, fs_name, MFSNAMELEN) == 0) {
			if (strncmp(fsinfo.f_fstypename, "ufs", MFSNAMELEN) != 0) {
				noasync = 1;
			}
		}
	}

	/* default to read-only mounts for 10.6 */
	if (!(mntflags & MNT_RDONLY)) {
		mntflags |= MNT_RDONLY;
	}

	if ((mntflags & MNT_UPDATE) == 0) {
		/* Check to see if UFS kext is already loaded */
		if (checkLoadable()) {
			/* load the kext */
			if (load_kmod()) {
				fprintf(stderr, "UFS filesystem is not available.\n");
				return 1;
			}
		}
	}

	/* default to async by setting the flag unless noasync was specified */
	if (mount("ufs", fs_name, mntflags, &args) < 0) {
		(void)fprintf(stderr, "%s on %s: ", args.fspec, fs_name);
		switch (errno) {
		case EMFILE:
			(void)fprintf(stderr, "mount table full.\n");
			break;
		case EINVAL:
			if (mntflags & MNT_UPDATE)
				(void)fprintf(stderr,
		    "Specified device does not match mounted device.\n");
			else 
				(void)fprintf(stderr,
				    "Incorrect super block.\n");
			break;
		default:
			(void)fprintf(stderr, "%s\n", strerror(errno));
			break;
		}
		return (1);
	}
	return (0);
}

void
ufs_usage()
{
	(void)fprintf(stderr, "usage: mount_ufs [-o options] special node\n");
	exit(1);
}
