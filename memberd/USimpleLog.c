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
#include <fcntl.h>		// for fcntl() and O_* flags
#include <stdarg.h>		// for va_*
#include <stdio.h>		// for FILE, vfprintf(), sprintf()
#include <string.h>		// for strerror()
#include <syslog.h>		// for vsyslog() et al
#include <time.h>		// for time() and localtime()
#include <sys/wait.h>	// for WIF*() et al


// ----------------------------------------------------------------------------
//	¥ Private Globals
// ----------------------------------------------------------------------------

// Used by dprintf() to determine if message should be sent to stderr.
// Set to false if redirected to /dev/null.
static bool				_StdIOIsValid = true ;

// To control logging options for dprintf().
static FILE				*_LogFile = 0 ;
#if DEBUG
static bool				_Debug = true ;
#else
static bool				_Debug = false ;
#endif


// ----------------------------------------------------------------------------
//	¥ Log File Functions (Private)
// ----------------------------------------------------------------------------
#pragma mark **** Log File Functions ****

static const char *_StdLogTimeString (
	time_t	inNow,
	char	*inTimeString )
{
	if (!inTimeString)
		return 0 ;
	struct tm		*tmTime = localtime (&inNow) ;

	sprintf (inTimeString, "%04d-%02d-%02d %02d:%02d:%02d %s",
				tmTime->tm_year + 1900, tmTime->tm_mon + 1, tmTime->tm_mday,
				tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec,
				tmTime->tm_zone) ;
	return inTimeString ;
}
/*
static void _CreateStdLogFile (
	const char	*inLogPath,
	const char	*inSoftware,
	const char	*inVersion )
{
	if (!inSoftware || !*inSoftware)
		inSoftware = "DAEMON" ;
	if (!inVersion || !*inVersion)
		inVersion = "10.x" ;

	// Setup the log file.
	if (NULL == (_LogFile = fopen (inLogPath, "a")))
		fprintf (stderr, "Could not open log file %s.\n", inLogPath) ;
	else {
		fcntl (fileno (_LogFile), F_SETFD, 1) ;

		// Add the file header only when first created.
		if (!ftell (_LogFile))
			fprintf (_LogFile, "#Version: 1.0\n#Software: %s, build %s\n",
							inSoftware, inVersion) ;

		// Always add time stamp and field ID.
		char	szTime [40] ;
		fprintf (_LogFile, "#Start-Date: %s\n"
						"#Fields: date time s-comment\n",
						_StdLogTimeString (time (NULL), szTime)) ;
		fflush (_LogFile) ;
	}
}
*/

static void _CloseLogFile (void)
{
	if (!_LogFile) {
		closelog () ;
		return ;
	}

	char	szTime [40] ;
	fprintf (_LogFile, "#End-Date: %s\n",
					_StdLogTimeString (time (NULL), szTime)) ;
	fclose (_LogFile) ;
}


// ----------------------------------------------------------------------------
//	¥ Logging Function (Public)
// ----------------------------------------------------------------------------
#pragma mark **** Logging Function ****

/*****
 *	dprintf() is wrapper for debugging / logging output.
 *****/
int dprintf (
	int			nSyslogPriority,
	const char	*szpFormat,
	... )
{
	va_list	args ;
	va_start (args, szpFormat) ;

	// If the facility hasn't been defined, make it LOG_DAEMON.
	if (!(nSyslogPriority & LOG_FACMASK))
		nSyslogPriority |= LOG_DAEMON ;

	register int	nResult = 0 ;
	time_t			tNow = time (NULL) ;
	struct tm		*tmTime = localtime (&tNow) ;
	char			szTime [40] ;

	sprintf (szTime, "%04d-%02d-%02d %02d:%02d:%02d %s\t",
				tmTime->tm_year + 1900, tmTime->tm_mon + 1, tmTime->tm_mday,
				tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec,
				tmTime->tm_zone) ;

	if (_LogFile) {
		fputs (szTime, _LogFile) ;
		nResult = vfprintf (_LogFile, szpFormat, args) ; 
		fflush (_LogFile) ;
	} else
		vsyslog (nSyslogPriority, szpFormat, args) ;

	// Log to stderr if the socket is valid
	//	AND ( we're debugging OR this is a high priority message )
	if (_StdIOIsValid
		&& (_Debug || (LOG_PRI (nSyslogPriority) <= LOG_WARNING))) {
		fputs (szTime, stderr) ;
		nResult = vfprintf (stderr, szpFormat, args) ;
		fflush (stderr) ;
	}
	return nResult ;
}

void LogExitStatus (
	pid_t			inChild,
	int				inStatus,
	const char		*inCommand )
{
	register size_t	nLen = 40 + (inCommand ? strlen (inCommand) : 0) ;
	char			szPidCmd [nLen] ;

	if (inCommand && *inCommand)
		sprintf (szPidCmd, "%d (\"%s\")", inChild, inCommand) ;
	else
		sprintf (szPidCmd, "%d", inChild) ;

	if (WIFEXITED (inStatus)) {
		dprintf (LOG_DEBUG, "Child process %s quit with exit status %d.\n",
					szPidCmd, WEXITSTATUS (inStatus)) ;
	} else if (WIFSIGNALED (inStatus)) {
		dprintf (LOG_DEBUG, "Child process %s terminated by signal %d.\n",
					szPidCmd, WTERMSIG (inStatus)) ;
	} else
		dprintf (LOG_DEBUG, "Reaped child process %s; status 0x%x.\n",
					szPidCmd, inStatus) ;
}
