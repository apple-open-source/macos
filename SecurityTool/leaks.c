/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
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
/*
 *  leaks.c
 *  security
 *
 *  Created by Michael Brouwer on Tue May 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "leaks.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int
leaks(int argc, char *const *argv)
{
	int result = 1;
	pid_t child;
	pid_t parent = getpid();

	child = fork();
	switch (child)
	{
	case -1:
		/* Fork failed we're hosed. */
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		break;
	case 0:
	{
		/* child. */
		char **argvec = (char **)malloc((argc + 2) * sizeof(char *));
		char pidstr[8];
		int ix;
	
		sprintf(pidstr, "%d", parent);
		argvec[0] = "/usr/bin/leaks";
		for (ix = 1; ix < argc; ++ix)
			argvec[ix] = argv[ix];
		argvec[ix] = pidstr;
		argvec[ix + 1] = NULL;

		execv(argvec[0], argvec);
		fprintf(stderr, "exec failed: %s\n", strerror(errno));
		exit(1);
		break;
	}
	default:
	{
		/* Parent. */
		int status = 0;
		for (;;)
		{
			/* Wait for the child to exit. */
			pid_t waited_pid = waitpid(child, &status, 0);
			if (waited_pid == -1)
			{
				int error = errno;
				/* Keep going if we get interupted but bail out on any
				   other error. */
				if (error == EINTR)
					continue;

				fprintf(stderr, "waitpid(%d) failed: %s\n", status, strerror(errno));
				break;
			}

			if (WIFEXITED(status))
			{
				if (WEXITSTATUS(status))
				{
					/* Force usage message. */
					result = 2;
					fprintf(stderr, "leaks exited %d\n", result);
				}
				break;
			}
			else if (WIFSIGNALED(status))
			{
				fprintf(stderr, "leaks terminated by signal %d\n", WTERMSIG(status));
				break;
			}
		}
		break;
	}
	}

	return result;
}
