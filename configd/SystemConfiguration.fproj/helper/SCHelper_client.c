/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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

#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "SCHelper_client.h"
#include "helper_comm.h"


#define	HELPER					"SCHelper"
#define	HELPER_LEN				(sizeof(HELPER) - 1)

#define	SUFFIX_SYM				"~sym"
#define	SUFFIX_SYM_LEN				(sizeof(SUFFIX_SYM) - 1)


__private_extern__
int
_SCHelperOpen(CFDataRef authorizationData)
{
	sigset_t			block;
	sigset_t			block_old;
	CFBundleRef			bundle;
	int				comm[2]		= { -1, -1 };
	int				exit_status	= 0;
	struct sigaction		ignore;
	struct sigaction		int_old;
	Boolean				ok		= FALSE;
	char				path[MAXPATHLEN]= { 0 };
	pid_t				pid1;
	struct sigaction		quit_old;
	uint32_t			status		= 0;
	CFURLRef			url		= NULL;
	static int			yes		= 1;

	// get CFBundleRef for SystemConfiguration.framework
	bundle = _SC_CFBundleGet();
	if (bundle != NULL) {
		url = CFBundleCopyResourceURL(bundle, CFSTR(HELPER), NULL, NULL);
	}

	if (url != NULL) {
		if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *)path, sizeof(path))) {
			path[0] = 0;
		}
		CFRelease(url);
	}

	// create tool<-->helper communications socket
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, comm) == -1) {
		perror("_SCHelperOpen socketpair() failed");
		return -1;
	}

	// ignore SIGINT, SIGQUIT
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags   = 0;
	(void)sigemptyset(&ignore.sa_mask);
	(void)sigaction(SIGINT , &ignore, &int_old );
	(void)sigaction(SIGQUIT, &ignore, &quit_old);

	// block SIGCHLD
	(void)sigemptyset(&block);
	(void)sigaddset(&block, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &block, &block_old);

	// fork
	pid1 = fork();
	if (pid1 == -1) {		// if error
		perror("_SCHelperOpen fork() failed");
		goto done;
	} else if (pid1 == 0) {		// if [first] child
		int	i;
		pid_t	pid2;

		// make sure that we don't step on syslog's FD (if open)
		closelog();

		// set stdin, stdout, stderr (and close other FD's)
		if (comm[0] != STDIN_FILENO) {
			(void)dup2(comm[0], STDIN_FILENO);
		}

		if (comm[0] != STDOUT_FILENO) {
			(void)dup2(comm[0], STDOUT_FILENO);
		}

		(void)close(STDERR_FILENO);
		(void)open(_PATH_CONSOLE, O_WRONLY, 0);

		for (i = getdtablesize() - 1; i > STDERR_FILENO; i--) {
			(void)close(i);
		}

		pid2 = vfork();
		if (pid2 == -1) {		// if error
			int	err	= errno;

			perror("_SCHelperOpen vfork() failed\n");
			(void)__SCHelper_txMessage(STDOUT_FILENO, err, NULL);
			_exit(err);
		} else if (pid2 == 0) {		// if [second] child
			char		*env;
			int		err	= ENOENT;
			size_t		len;

			// restore signal processing
			(void)sigaction(SIGINT , &int_old , NULL);
			(void)sigaction(SIGQUIT, &quit_old, NULL);
			(void)sigprocmask(SIG_SETMASK, &block_old, NULL);

			if (path[0] != 0) {
				(void)execl(path, path, NULL);
				err = errno;
			}

			// if appropriate (e.g. when debugging), try a bit harder

			env = getenv("DYLD_FRAMEWORK_PATH");
			len = (env != NULL) ? strlen(env) : 0;

			// trim any trailing slashes
			while ((len > 1) && (env[len - 1] == '/')) {
				len--;
			}

			// if DYLD_FRAMEWORK_PATH is ".../xxx~sym" than try ".../xxx~sym/SCHelper"
			if ((len > SUFFIX_SYM_LEN) &&
			    (strncmp(&env[len - SUFFIX_SYM_LEN], SUFFIX_SYM, SUFFIX_SYM_LEN) == 0) &&
			    ((len + 1 + HELPER_LEN) < MAXPATHLEN)) {
				char		path[MAXPATHLEN];

				strlcpy(path, env, sizeof(path));
				strlcpy(&path[len], "/", sizeof(path) - (len - 1));
				strlcat(&path[len], HELPER, sizeof(path) - len);

				(void)execl(path, path, NULL);
				err = errno;
			}

			// if SCHelper could not be started
			(void)__SCHelper_txMessage(STDOUT_FILENO, err, NULL);
			_exit(err != 0 ? err : ENOENT);
		}

		// [first] child
		_exit(0);
	}

	if (wait4(pid1, &exit_status, 0, NULL) == -1) {
		perror("_SCHelperOpen wait4() failed");
		goto done;
	}

	if (WIFEXITED(exit_status)) {
		if (WEXITSTATUS(exit_status) != 0) {
			SCLog(TRUE, LOG_INFO,
			      CFSTR("could not start \"" HELPER "[1]\", exited w/status = %d"),
			      WEXITSTATUS(exit_status));
			goto done;
		}
	} else if (WIFSIGNALED(exit_status)) {
		SCLog(TRUE, LOG_INFO,
		      CFSTR("could not start \"" HELPER "[1]\", terminated w/signal = %d"),
		      WTERMSIG(exit_status));
		goto done;
	} else {
		SCLog(TRUE, LOG_INFO,
		      CFSTR("could not start \"" HELPER "[1]\", exit_status = %x"),
		      exit_status);
		goto done;
	}

	(void)close(comm[0]);
	comm[0] = -1;

	if (setsockopt(comm[1], SOL_SOCKET, SO_NOSIGPIPE, (const void *)&yes, sizeof(yes)) == -1) {
		perror("_SCHelperOpen setsockopt() failed");
		goto done;
	}

	ok = __SCHelper_rxMessage(comm[1], &status, NULL);
	if (!ok) {
		SCLog(TRUE, LOG_INFO, CFSTR("could not start \"" HELPER "\", no status available"));
		goto done;
	}

	ok = (status == 0);
	if (!ok) {
		SCLog(TRUE, LOG_INFO, CFSTR("could not start \"" HELPER "\", status = %u"), status);
		goto done;
	}

	ok = _SCHelperExec(comm[1], SCHELPER_MSG_AUTH, authorizationData, &status, NULL);
	if (!ok) {
		SCLog(TRUE, LOG_INFO, CFSTR("_SCHelperOpen: could not send authorization"));
		goto done;
	}

    done :

	// restore signal processing
	(void)sigaction(SIGINT , &int_old , NULL);
	(void)sigaction(SIGQUIT, &quit_old, NULL);
	(void)sigprocmask(SIG_SETMASK, &block_old, NULL);

	if (comm[0] > 0) {
		(void)close(comm[0]);
//		comm[0] = -1;
	}

	if (!ok) {
		(void)close(comm[1]);
		comm[1] = -1;
	}

	return comm[1];
}


__private_extern__
void
_SCHelperClose(int helper)
{
	if (!_SCHelperExec(helper, SCHELPER_MSG_EXIT, NULL, NULL, NULL)) {
		SCLog(TRUE, LOG_INFO, CFSTR("_SCHelperOpen: could not send exit request"));
	}

	(void)close(helper);
	return;
}
