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

/* 
 * Modification History 
 * 24 March 2000	Allan Nathanson (ajn@apple.com)
 *			- created
 */

#include <stdio.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>
#include <paths.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <objc/objc-runtime.h>

#include "configd.h"
#include "configd_server.h"
#include "plugin_support.h"


const char *signames[] = {
	""    , "HUP" , "INT"   , "QUIT", "ILL"  , "TRAP", "ABRT", "EMT" ,
	"FPE" , "KILL", "BUS"   , "SEGV", "SYS"  , "PIPE", "ALRM", "TERM",
	"URG" , "STOP", "TSTP"  , "CONT", "CHLD" , "TTIN", "TTOU", "IO"  ,
	"XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "INFO", "USR1", "USR2"
};


void
usage(const char *prog)
{
	SCDLog(LOG_INFO, CFSTR("%s: [-d] [-v] [-b] [-t plugin-bundle-path]"), prog);
	SCDLog(LOG_INFO, CFSTR("options:"));
	SCDLog(LOG_INFO, CFSTR("\t-d\tenable debugging"));
	SCDLog(LOG_INFO, CFSTR("\t-v\tenable verbose logging"));
	SCDLog(LOG_INFO, CFSTR("\t-b\tdisable loading of ALL plug-ins"));
	SCDLog(LOG_INFO, CFSTR("\t-t\tload/test the specified plug-in"));
	SCDLog(LOG_INFO, CFSTR("\t\t  (Note: only the plug-in will be started)"), prog);
	exit (EX_USAGE);
}


void
catcher(int signum)
{
	/*
	 * log the signal
	 *
	 * Note: we can't use SCDLog() since the signal may be received while the
	 *       logging thread lock is held.
	 */
	if (SCDOptionGet(NULL, kSCDOptionUseSyslog)) {
		syslog (LOG_INFO, "caught SIG%s"  , signames[signum]);
	} else {
		fprintf(stderr,   "caught SIG%s\n", signames[signum]);
		fflush (stderr);
	}

	return;
}

static void
parent_exit(int i)
{
	_exit (0);
}

static int
fork_child()
{
	int	child_pid;
	int	fd;

	signal(SIGTERM, parent_exit);
	child_pid = fork();
	switch (child_pid) {
		case -1: {
			return -1;
		}
		case 0: {
			/* child: becomes the daemon (see below) */
			signal(SIGTERM, SIG_DFL);
			break;
		}
		default: {
			/* parent: wait for signal, then exit */
			int	status;

			(void) wait4(child_pid, (int *)&status, 0, 0);
			if (WIFEXITED(status)) {
				fprintf(stderr,
					"*** configd (daemon) failed to start, exit status=%d",
					WEXITSTATUS(status));
			} else {
				fprintf(stderr,
					"*** configd (daemon) failed to start, received signal=%d",
					WTERMSIG(status));
			}
			fflush (stderr);
			exit (EX_SOFTWARE);
		}
	}

	if (setsid() == -1)
		return -1;

	(void)chdir("/");

	if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)close(fd);
	}

	return 0;
}

int
main (int argc, const char *argv[])
{
	extern int		optind;
	int			opt;
	const char		*prog = argv[0];
	boolean_t		loadBundles = TRUE;
	const char		*testBundle = NULL;
	struct sigaction	nact;

	/* process any arguments */

	while ((opt = getopt(argc, argv, "bdt:v")) != -1) {
		switch(opt) {
			case 'b':
				loadBundles = FALSE;
				break;
			case 'd':
				SCDOptionSet(NULL, kSCDOptionDebug, TRUE);
				break;
			case 't':
				testBundle = optarg;
				break;
			case 'v':
				SCDOptionSet(NULL, kSCDOptionVerbose, TRUE);
				break;
			case '?':
			default :
				usage(prog);
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * display an error if configd is already running and we are
	 * not executing/testing a bundle.
	 */
	if ((testBundle == NULL) && (server_active() == TRUE)) {
		exit (EX_UNAVAILABLE);
	}

	/* get ready */

	SCDOptionSet(NULL, kSCDOptionIsServer, TRUE);		/* Use the config API's "server" code */
	SCDOptionSet(NULL, kSCDOptionUseCFRunLoop, TRUE);	/* Use the CFRunLoop */

	/* check credentials */
	if (getuid() != 0) {
#ifdef	DEBUG
		if (!SCDOptionGet(NULL, kSCDOptionDebug)) {
#endif	/* DEBUG */
			fprintf(stderr, "%s: permission denied.\n", prog);
			exit (EX_NOPERM);
#ifdef	DEBUG
		}
#endif	/* DEBUG */
	}

	if ((testBundle == NULL) && !SCDOptionGet(NULL, kSCDOptionDebug)) {
		if (fork_child() == -1) {
			fprintf(stderr, "configd: fork() failed, %s\n", strerror(errno));
			exit (1);
		}
		/* now the child process, parent waits in fork_child */

		/* log via syslog() facility */
		openlog("configd", (LOG_NDELAY | LOG_PID), LOG_DAEMON);
		SCDOptionSet(NULL, kSCDOptionUseSyslog, TRUE);
	}

	/* add signal handler to catch a SIGPIPE */

	nact.sa_handler = catcher;
	sigemptyset(&nact.sa_mask);
	nact.sa_flags = SA_RESTART;

	if (sigaction(SIGPIPE, &nact, NULL) == -1) {
		SCDLog(LOG_ERR,
		       CFSTR("sigaction(SIGPIPE, ...) failed: %s"),
		       strerror(errno));
	}

	/* get set */

	objc_setMultithreaded(YES);

	if (testBundle == NULL) {
		/* initialize primary (cache management) thread */
		server_init();

		/* load/initialize/start bundles into the secondary thread */
		if (loadBundles) {
			plugin_init();
		} else {
			if (!SCDOptionGet(NULL, kSCDOptionDebug)) {
			    /* synchronize with parent process */
			    kill(getppid(), SIGTERM);
			}
		}
	}

	/* go */

	if (testBundle == NULL) {
		/* start primary (cache management) thread */
		server_loop();
	} else {
		/* load/initialize/start specified plug-in */
		plugin_exec((void *)testBundle);
	}

	exit (EX_OK);	// insure the process exit status is 0
	return 0;	// ...and make main fit the ANSI spec.
}
