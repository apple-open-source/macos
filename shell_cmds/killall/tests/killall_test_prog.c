/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{

	if (argc == 1) {
		errx(1, "expected filename");
	} else if (strcmp(argv[1], "--child") != 0) {
		/*
		 * Pass on the argument, which should be the name of a flag
		 * file that the test will wait on, to the child to create.
		 */
		char * const nargv[] = { "innocent_test_prog",
		   "--child", argv[1], NULL };
		char * const envp[] = { NULL };

		execve(argv[0], nargv, envp);
		err(1, "execve");
	} else {
		int fd;

		argc -= 2;
		argv += 2;

		if (argc == 0)
			errx(1, "expected filename after --child");

		fd = open(argv[0], O_RDWR | O_CREAT, 0755);
		if (fd < 0)
			err(1, "%s", argv[0]);
		close(fd);

		while (1) {
			printf("Awaiting termination... ");
			sleep(1);
		}
	}

}
