/*
 * Copyright (c) 2004 Research Engineering Development.
 * Author: Alfred Perlstein <alfred@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: uid_driver.c,v 1.2 2004/05/27 08:07:49 pwd Exp $
 */


#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/mount.h>

#include "sysctl.h"
#include "autofs.h"

const char *progname;
int opt_z;

void dotheneedful(uid_t uid, const char *fname);
void usage(void);

int
main(int argc, char **argv)
{
	struct autofs_mounterreq mr;
	struct stat sb;
	struct statfs sfs;
	const char *fname;
	int ch, i, status, x;
	pid_t pid;

	progname = argv[0];

	while ((ch = getopt(argc, argv, "z")) != -1) {
		switch (ch) {
		case 'z':
			opt_z = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	fname = argv[0];

	/* statfs and sysctl_fsid require root. */
	if (geteuid() != 0)
		err(1, "you must be superuser to run this");

	if (fname == NULL)
		usage();

	if (stat(fname, &sb) == -1)
		err(1, "stat: %s", fname);

	if (opt_z == 0) {
		if (statfs(fname, &sfs) == -1)
			err(1, "statfs: %s", fname);
		if (strcmp(sfs.f_fstypename, "autofs") != 0)
			errx(1, "path %s is not an autofs filesystem", fname);
		bzero(&mr, sizeof(mr));
		mr.amu_flags = AUTOFS_MOUNTERREQ_UID;
		mr.amu_ino = sb.st_ino;
		x = sysctl_fsid(AUTOFS_CTL_TRIGGER, &sfs.f_fsid, NULL, 0,
		    &mr, sizeof(mr));
		if (x == -1)
			err(1, "sysctl_fsid AUTOFS_CTL_TRIGGER %s", fname);
	}

	printf("parent forking children...\n");
	fflush(NULL);
	for (i = 1; i < 5; i++) {
		dotheneedful(i, fname);
	}
	for ( ;; ) {
		pid = wait(&status);
		if (pid == -1) {
			switch (errno) {
			case EINTR:
				continue;
			case ECHILD:
				goto out;
			default:
				err(1, "wait");
			}
		}
	}
out:

	return (0);
}

void
dotheneedful(uid_t uid, const char *fname)
{
	pid_t pid;
	struct stat sb;

	pid = fork();
	if (pid == -1)
		err(1, "fork");

	if (pid != 0)
		return;

	if (seteuid(uid) != 0)
		err(1, "child: seteuid %ld", (long)uid);
	/* child */
	if (stat(fname, &sb) != 0)
		err(1, "child: stat %s", fname);
	printf("child: pid %ld, uid %ld, ino %ld\n",
	    (long)getpid(), (long)geteuid(), (long)sb.st_ino);
	exit(0);
}

void
usage(void)
{

	fprintf(stderr, "usage: %s [-z] dirname", progname);
	exit(1);
}
