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

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static _SCDFlags	globalFlags = {
	FALSE		/* debug        */,
	FALSE		/* verbose      */,
	FALSE		/* useSyslog    */,
	FALSE		/* isLocked     */,
	TRUE		/* useCFRunLoop */,
};

static boolean_t	isSCDServer = FALSE;

static const struct scd_errmsg {
	SCDStatus	status;
	char		*message;
} scd_errmsgs[] = {
	{ SCD_OK,		"Success!" },
	{ SCD_NOSESSION,	"Configuration daemon session not active" },
	{ SCD_NOSERVER,		"Configuration daemon not (no longer) available" },
	{ SCD_LOCKED,		"Lock already held" },
	{ SCD_NEEDLOCK,		"Lock required for this operation" },
	{ SCD_EACCESS,		"Permission denied (must be root to obtain lock)" },
	{ SCD_NOKEY,		"No such key" },
	{ SCD_EXISTS,		"Data associated with key already defined" },
	{ SCD_STALE,		"Write attempted on stale version of object" },
	{ SCD_INVALIDARGUMENT,	"Invalid argument" },
	{ SCD_NOTIFIERACTIVE,	"Notifier is currently active" },
	{ SCD_FAILED,		"Failed!" }
};
#define nSCD_ERRMSGS (sizeof(scd_errmsgs)/sizeof(struct scd_errmsg))


int
SCDOptionGet(SCDSessionRef session, const int option)
{
	_SCDFlags	*theFlags = &globalFlags;
	int		value     = 0;

	if (session != NULL) {
		theFlags = &((SCDSessionPrivateRef)session)->flags;
	}

	switch (option) {

		/* session dependent flags */

		case kSCDOptionDebug :
			value = theFlags->debug;
			break;

		case kSCDOptionVerbose :
			value = theFlags->verbose;
			break;

		case kSCDOptionIsLocked :
			value = theFlags->isLocked;
			break;

		case kSCDOptionUseSyslog :
			value = theFlags->useSyslog;
			break;

		case kSCDOptionUseCFRunLoop :
			value = theFlags->useCFRunLoop;
			break;

		/* session independent flags */

		case kSCDOptionIsServer :
			value = isSCDServer;
			break;
	}

	return value;
}


void
SCDOptionSet(SCDSessionRef session, int option, const int value)
{
	_SCDFlags	*theFlags = &globalFlags;

	if (session != NULL) {
		theFlags = &((SCDSessionPrivateRef)session)->flags;
	}

	switch (option) {

		/* session independent flags */

		case kSCDOptionDebug :
			theFlags->debug = value;
			break;

		case kSCDOptionVerbose :
			theFlags->verbose = value;
			break;

		case kSCDOptionIsLocked :
			theFlags->isLocked = value;
			break;

		case kSCDOptionUseSyslog :
			theFlags->useSyslog = value;
			break;

		case kSCDOptionUseCFRunLoop :
			theFlags->useCFRunLoop = value;
			break;

		/* session independent flags */

		case kSCDOptionIsServer :
			isSCDServer = value;
			break;
	}

	return;
}


static void
_SCDLog(SCDSessionRef session, int level, CFArrayRef lines)
{
	FILE		*f = (LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout;
	CFDataRef	line;
	int		i;

	if ((LOG_PRI(level) == LOG_DEBUG) && !SCDOptionGet(session, kSCDOptionVerbose)) {
		/* it's a debug message and we haven't requested verbose logging */
		return;
	}

	pthread_mutex_lock(&lock);

	for (i=0; i<CFArrayGetCount(lines); i++) {
		line = CFStringCreateExternalRepresentation(NULL,
							    CFArrayGetValueAtIndex(lines, i),
							    kCFStringEncodingMacRoman,
							    '?');
		if (line != NULL) {
			if (SCDOptionGet(session, kSCDOptionUseSyslog)) {
				syslog (level,  "%.*s",   (int)CFDataGetLength(line), CFDataGetBytePtr(line));
			} else {
				fprintf(f, "%.*s\n", (int)CFDataGetLength(line), CFDataGetBytePtr(line));
				fflush (f);
			}
			CFRelease(line);
		}
	}

	pthread_mutex_unlock(&lock);
}


void
SCDSessionLog(SCDSessionRef session, int level, CFStringRef formatString, ...)
{
	va_list		argList;
	CFStringRef	resultString;
	CFArrayRef	lines;

	va_start(argList, formatString);
	resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
	va_end(argList);

	lines = CFStringCreateArrayBySeparatingStrings(NULL, resultString, CFSTR("\n"));
	_SCDLog(session, level, lines);
	CFRelease(lines);
	CFRelease(resultString);
}


void
SCDLog(int level, CFStringRef formatString, ...)
{
	va_list		argList;
	CFStringRef	resultString;
	CFArrayRef	lines;

	va_start(argList, formatString);
	resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
	va_end(argList);

	lines = CFStringCreateArrayBySeparatingStrings(NULL, resultString, CFSTR("\n"));
	_SCDLog(NULL, level, lines);
	CFRelease(lines);
	CFRelease(resultString);
}


const char *
SCDError(SCDStatus status)
{
	int i;

	for (i = 0; i < nSCD_ERRMSGS; i++) {
		if (scd_errmsgs[i].status == status) {
			return scd_errmsgs[i].message;
		}
	}
	return "(unknown error)";
}
