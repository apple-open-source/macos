/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <unistd.h>
#include <signal.h>

char *warning = "\
#\n\
# Warning - this file should only be modified with vifs(8)\n\
#\n\
# Failure to do so is unsupported and may be destructive.\n\
#\n";

int
main(int argc, char *argv[])
{
	struct stat sb;
	int fd, x;
	uid_t euid;
	pid_t editpid;
	char *p, *editor;

	if (argc != 1) {
		printf("usage: vifs\n");
		exit(1);
	}

	euid = geteuid();
	if (euid != 0)
		errx(1, "need to run as root");

	/* examine the existing fstab, try to create it if needed */
	if (stat(_PATH_FSTAB, &sb) < 0) {
		if (errno == ENOENT) {
			fd = open(_PATH_FSTAB, O_CREAT | O_WRONLY,
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (fd < 0)
				errx(1, "error creating %s", _PATH_FSTAB);
			write(fd, warning, strlen(warning));
			close(fd);
		} else {
			errx(1, "could not stat %s", _PATH_FSTAB);
		}
	}

	/* prepare the file for the editor */
	fd = open(_PATH_FSTAB, O_RDONLY, 0);
	if (fd < 0)
		errx(1, "error opening %s", _PATH_FSTAB);

	x = fcntl(fd, F_SETFD, 1);
	if (x < 0)
		errx(1, "error setting close on exit");

	x = flock(fd, LOCK_EX | LOCK_NB);
	if (x != 0)
		errx(1, "file is busy");

	/* obtain and invoke the editor */
	editor = getenv("EDITOR");
	if (editor == NULL)
		editor = _PATH_VI;
	p = strrchr(editor, '/');
        if (p != NULL)
		++p;
	else
		p = editor;

	editpid = vfork();
	if (editpid == 0) {
		execlp(editor, p, _PATH_FSTAB, NULL);
		_exit(1);
	}

	for ( ; ; ) {
		editpid = waitpid(editpid, (int *)&x, WUNTRACED);
		if (editpid == -1)
			errx(1, "editing error");
		else if (WIFSTOPPED(x))
			raise(WSTOPSIG(x));
		else if (WIFEXITED(x) && WEXITSTATUS(x) == 0)
			break;
                else
			errx(1, "editing error");
	}

	/* let process death clean up locks and file descriptors */
	return 0;
}
