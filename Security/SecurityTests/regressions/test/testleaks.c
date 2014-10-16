/*
 * Copyright (c) 2003-2006 Apple Computer, Inc. All Rights Reserved.
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
 * testleaks.c
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "testleaks.h"
#include "testmore.h"

#if 0
static char *cf_user_text_encoding_var;
#endif

int
test_leaks(void)
{
	return 0;
#if 0
	int leaks = 0;
	pid_t child;
	pid_t parent;

	setup("leaks");

	/* Work around the fact that CF calls setenv, which leaks. */
	cf_user_text_encoding_var = getenv("__CF_USER_TEXT_ENCODING");

	ok_unix(parent = getpid(), "getpid");
	int cld_stdout[2] = {};
	ok_unix(child = pipe(cld_stdout), "pipe");
	ok_unix(child = fork(), "fork");
	switch (child)
	{
	case -1:
		break;
	case 0:
	{
		/* child. */

		/* Set childs stdout and stderr to pipe. */
		ok_unix(close(cld_stdout[0]), "close parent end of pipe");
		ok_unix(dup2(cld_stdout[1], 1), "reopen stdout on pipe");
#if 0
		ok_unix(dup2(cld_stdout[1], 2), "reopen stderr on pipe");
#endif

		int argc = 0;
		char *const *argv = NULL;
		char **argvec = (char **)malloc((argc + 2) * sizeof(char *));
		char pidstr[8];
		int ix;
	
		sprintf(pidstr, "%d", parent);
		argvec[0] = "/usr/bin/leaks";
		for (ix = 1; ix < argc; ++ix)
			argvec[ix] = argv[ix];
		argvec[ix] = pidstr;
		argvec[ix + 1] = NULL;

		ok_unix(execv(argvec[0], argvec), "execv");
		_exit(1);
		break;
	}
	default:
	{
		/* Parent. */
		ok_unix(close(cld_stdout[1]), "close child end of pipe");

		/* Set statemachine initial state to 0. */
		int state = 0;
		/* True iff the last char read was a newline. */
		int newline = 1;
		char buf[4098];
		for (;;)
		{
			char *p = buf + 2;
			ssize_t bytes_read;
			bytes_read = read(cld_stdout[0], p, 4096);
			if (bytes_read <= 0)
				break;

			int start = newline ? -2 : 0;
			int ix = 0;
			for (ix = 0; ix < bytes_read; ++ix)
			{
				/* Simple state machine for parsing leaks output.
				 * Looks for
				 *     '[^\n]*\n[^:]*: ([0-9]*)'
				 * and sets leaks to atoi of the ([0-9]*) bit. */
				switch (state)
				{
				case 0: if (p[ix] == '\n') state = 1; break;
				case 1: if (p[ix] == ':') state = 2; break;
				case 2: if (p[ix] == ' ') state = 3; break;
				case 3:
					if (p[ix] <= '0' || p[ix] >='9')
						state = 4;
					else
						leaks = leaks * 10 + p[ix] - '0';
					break;
				case 4: break;
				}

				/* If there is a newline in the input or we are looking
				   at the last char of the buffer it's time to write the
				   output. */
				if (p[ix] == '\n' || ix + 1 >= bytes_read)
				{
					/* If the previous char was a newline we prefix the
					   output with "# ". */
					if (newline)
					{
						p[start] = '#';
						p[start + 1] = ' ';
					}
					fwrite(p + start, ix + 1 - start, 1, stdout);
					if (p[ix] == '\n')
					{
						start = ix - 1;
						newline = 1;
					}
					else
						newline = 0;
				}
			}
		}

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

				ok_unix(waited_pid, "waitpid");
				break;
			}

			if (WIFEXITED(status))
			{
				is(WEXITSTATUS(status), 0, "leaks exit status");
				break;
			}
			else if (WIFSIGNALED(status))
			{
				is(WTERMSIG(status), 0, "leaks terminated by");
				break;
			}
		}
		break;
	}
	}

	return leaks;
#endif
}
