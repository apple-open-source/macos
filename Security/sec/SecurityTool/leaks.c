/*
 * Copyright (c) 2003-2004,2006-2007 Apple Inc. All Rights Reserved.
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
 *
 * leaks.c
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
		fprintf(stderr, "fork: %s", strerror(errno));
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
		fprintf(stderr, "exec: %s", strerror(errno));
		_exit(1);
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

				fprintf(stderr, "waitpid %d: %s", status, strerror(errno));
				break;
			}

			if (WIFEXITED(status))
			{
				if (WEXITSTATUS(status))
				{
					/* Force usage message. */
					result = 2;
					fprintf(stderr, "leaks exited: %d", result);
				}
				break;
			}
			else if (WIFSIGNALED(status))
			{
				fprintf(stderr, "leaks terminated by signal: %d", WTERMSIG(status));
				break;
			}
		}
		break;
	}
	}

	return result;
}
