/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * 24 March 2000		Allan Nathanson (ajn@apple.com)
 * - created
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

Boolean	_configd_fork			= TRUE;		/* TRUE if process should be run in the background */
Boolean	_configd_verbose		= FALSE;	/* TRUE if verbose logging enabled */
CFMutableSetRef	_plugins_exclude	= NULL;		/* bundle identifiers to exclude from loading */
CFMutableSetRef	_plugins_verbose	= NULL;		/* bundle identifiers to enable verbose logging */

static const char *signames[] = {
	""    , "HUP" , "INT"   , "QUIT", "ILL"  , "TRAP", "ABRT", "EMT" ,
	"FPE" , "KILL", "BUS"   , "SEGV", "SYS"  , "PIPE", "ALRM", "TERM",
	"URG" , "STOP", "TSTP"  , "CONT", "CHLD" , "TTIN", "TTOU", "IO"  ,
	"XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "INFO", "USR1", "USR2"
};


static void
usage(const char *prog)
{
	SCPrint(TRUE, stderr, CFSTR("%s: [-d] [-v] [-V bundleID] [-b] [-B bundleID] [-t plugin-path]\n"), prog);
	SCPrint(TRUE, stderr, CFSTR("options:\n"));
	SCPrint(TRUE, stderr, CFSTR("\t-d\tdisable daemon/run in foreground\n"));
	SCPrint(TRUE, stderr, CFSTR("\t-v\tenable verbose logging\n"));
	SCPrint(TRUE, stderr, CFSTR("\t-V\tenable verbose logging for the specified plug-in\n"));
	SCPrint(TRUE, stderr, CFSTR("\t-b\tdisable loading of ALL plug-ins\n"));
	SCPrint(TRUE, stderr, CFSTR("\t-B\tdisable loading of the specified plug-in\n"));
	SCPrint(TRUE, stderr, CFSTR("\t-t\tload/test the specified plug-in\n"));
	SCPrint(TRUE, stderr, CFSTR("\t\t  (Note: only the plug-in will be started)\n"), prog);
	exit (EX_USAGE);
}


static void
catcher(int signum)
{
	/*
	 * log the signal
	 *
	 * Note: we can't use SCLog() since the signal may be received while the
	 *       logging thread lock is held.
	 */
	if (_configd_verbose) {
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

	(void) chdir("/");

	fd = open(_PATH_DEVNULL, O_RDWR, 0);
	if (fd != -1) {
		int	ofd;

		// stdin
		(void) dup2(fd, STDIN_FILENO);

		// stdout, stderr
		ofd = open("/var/log/configd.log", O_WRONLY|O_APPEND, 0);
		if (ofd != -1) {
			if (fd > STDIN_FILENO) {
				(void) close(fd);
			}
			fd = ofd;
		}
		(void) dup2(fd, STDOUT_FILENO);
		(void) dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) {
			(void) close(fd);
		}
	}

	return 0;
}


static void
writepid(void)
{
	FILE *fp;

	fp = fopen("/var/run/configd.pid", "w");
	if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}


int
main(int argc, char * const argv[])
{
	Boolean			loadBundles = TRUE;
	struct sigaction	nact;
	int			opt;
	extern int		optind;
	const char		*prog = argv[0];
	CFStringRef		str;
	const char		*testBundle = NULL;

	_plugins_exclude = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	_plugins_verbose = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

	/* process any arguments */

	while ((opt = getopt(argc, argv, "bB:dt:vV:")) != -1) {
		switch(opt) {
			case 'b':
				loadBundles = FALSE;
				break;
			case 'B':
				str = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingMacRoman);
				CFSetSetValue(_plugins_exclude, str);
				CFRelease(str);
				break;
			case 'd':
				_configd_fork = FALSE;
				break;
			case 't':
				testBundle = optarg;
				break;
			case 'v':
				_configd_verbose = TRUE;
				break;
			case 'V':
				if (strcmp(optarg, "com.apple.SystemConfiguration") == 0) {
					_sc_verbose = TRUE;
				} else {
				str = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingMacRoman);
				CFSetSetValue(_plugins_verbose, str);
				CFRelease(str);
				}
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

	/* check credentials */
	if (getuid() != 0) {
		fprintf(stderr, "%s: permission denied.\n", prog);
		exit (EX_NOPERM);
	}

	if (_configd_fork) {
		if (fork_child() == -1) {
			fprintf(stderr, "configd: fork() failed, %s\n", strerror(errno));
			exit (1);
		}
		/* now the child process, parent waits in fork_child */
	}

	/* record process id */
	if (testBundle == NULL) {
		writepid();
	}

	/* open syslog() facility */
	if (_configd_fork) {
		int	logopt	= LOG_NDELAY|LOG_PID;

		if (_configd_verbose)
			logopt |= LOG_CONS;
		openlog("configd", logopt, LOG_DAEMON);
	} else {
		_sc_log = FALSE;	/* redirect SCLog() to stdout/stderr */
	}

	/* add signal handler to catch a SIGHUP */

	nact.sa_handler = catcher;
	sigemptyset(&nact.sa_mask);
	nact.sa_flags = SA_RESTART;

	if (sigaction(SIGHUP, &nact, NULL) == -1) {
		SCLog(_configd_verbose, LOG_ERR,
		       CFSTR("sigaction(SIGHUP, ...) failed: %s"),
		       strerror(errno));
	}

	/* add signal handler to catch a SIGPIPE */

	if (sigaction(SIGPIPE, &nact, NULL) == -1) {
		SCLog(_configd_verbose, LOG_ERR,
		       CFSTR("sigaction(SIGPIPE, ...) failed: %s"),
		       strerror(errno));
	}

	/* get set */

	objc_setMultithreaded(YES);

	if (testBundle == NULL) {
		/* initialize primary (store management) thread */
		server_init();

		/* load/initialize/start bundles into the secondary thread */
		if (loadBundles) {
			plugin_init();
		} else {
			if (_configd_fork) {
			    /* synchronize with parent process */
			    kill(getppid(), SIGTERM);
			}
		}
	}

	/* go */

	if (testBundle == NULL) {
		/* start primary (store management) thread */
		server_loop();
	} else {
		/* load/initialize/start specified plug-in */
		plugin_exec((void *)testBundle);
	}

	exit (EX_OK);	// insure the process exit status is 0
	return 0;	// ...and make main fit the ANSI spec.
}
