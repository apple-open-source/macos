/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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


// ANSI / POSIX headers
#include <errno.h>			// for errno
#include <fcntl.h>			// for fcntl() and O_* flags
#include <limits.h>			// for ARG_MAX, OPEN_MAX and PATH_MAX
#include <paths.h>			// for _PATH_DEVNULL
#include <stdlib.h>			// for atexit() and exit()
#include <string.h>			// for strerror()
#include <unistd.h>			// for exec(), getpid(), open(), select() et al
#include <sys/time.h>		// for struct timeval (for sys/resource.h)
#include <sys/resource.h>	// for getrlimit()


// ----------------------------------------------------------------------------
//	¥ Private Globals
// ----------------------------------------------------------------------------

// Used to comunicate with dprintf():
// static bool				_StdIOIsValid ;

// ----------------------------------------------------------------------------
//	¥ Main Helper Functions (Private)
// ----------------------------------------------------------------------------
#pragma mark **** Main Helper Functions ****

static void _Detach ( void )
{
	errno = 0 ;
	switch (fork ()) {
		case 0:		// in child
			break ;
		case -1:	// error
			fprintf (stderr, "fork() failed: errno=%d\n", errno) ;
			/* FALLTHRU */
		default:	// parent
			exit (errno) ;
	}
}

/*****
 *	_RedirectStdFile(): Redirect standard file descriptors to existing
 *	file descriptors, or /dev/null.
 *****/
static void _RedirectStdFile (
	int	inNewStdin,
	int	inNewStdout,
	int	inNewStderr )
{
	register int	i = -1 ;

	// This is messy because it's flexible. One key point is that we can
	// NOT close the passed fd's.

	// First, is this the default case, i.e. redirect to /dev/null?
	if (inNewStdin == -1)
		inNewStdin = i = open (_PATH_DEVNULL, O_RDONLY) ;
	// OK, redirect to the requested fd, but only if it's not already there.
	if ((inNewStdin != -1) && (inNewStdin != STDIN_FILENO)) {
		dup2 (inNewStdin, STDIN_FILENO) ;
		// Finally, close /dev/null if we opened it AND it wasn't stdin's fd.
		if ((i != -1) && (i != STDIN_FILENO))
			close (i) ;
		i = -1 ;
	}

	// Handle stdout and stderr the same way, but we've got two.
	if ((inNewStdout == -1) || (inNewStderr == -1)) {
		i = open (_PATH_DEVNULL, O_WRONLY) ;
#if USE_DPRINTF
		_StdIOIsValid = false ;
#endif	/* USE_DPRINTF */
	}
	if (inNewStdout == -1)
		inNewStdout = i ;
	if (inNewStdout != STDOUT_FILENO)
		dup2 (inNewStdout, STDOUT_FILENO) ;

	if (inNewStderr == -1)
		inNewStderr = i ;
	if (inNewStderr != STDERR_FILENO)
		dup2 (inNewStderr, STDERR_FILENO) ;

	if ((i != -1) && (i != STDOUT_FILENO) && (i != STDERR_FILENO))
		close (i) ;
}

/*****
 *	_CloseFileDescriptors(): Closes all file descriptors for this process.
 *	Will also redirect standard file descriptors to /dev/null if requested.
 *****/
static void _CloseFileDescriptors ( bool inRedirectStdFile )
{
	// Find the true upper limit on file descriptors.
	register int	i = OPEN_MAX ;
	register int	nMin = (inRedirectStdFile ? -1 : STDERR_FILENO) ;
	struct rlimit	lim ;

	if (!getrlimit (RLIMIT_NOFILE, &lim))
		i = lim.rlim_cur ;
#if DEBUG
	else
		fprintf (stderr, "_CloseFileDescriptors(): getrlimit() failed.\n") ;
#endif

	// Close all file descriptors except std*.
	while (--i > nMin)
		close (i) ;
	if (inRedirectStdFile)
		_RedirectStdFile (-1, -1, -1) ;
}

static const char *__PidPath = NULL ;

static void _ErasePidFile ( void )
{
	if (__PidPath)
		unlink (__PidPath) ;
}

/*****
 *	_WritePidFile(): writes out standard pid file at given path.
 *****/
static void _WritePidFile ( const char *inPidPath )
{
	FILE *fp = fopen (inPidPath, "w") ;
	if (fp == NULL)
		return ;
	fprintf (fp, "%d\n", getpid ()) ;
	fclose (fp) ;
	__PidPath = inPidPath ;
	atexit (_ErasePidFile) ;
}
