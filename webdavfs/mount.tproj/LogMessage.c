/*
	File:		LogMessage.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 2001-2011 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(bms)	Brad Suinn

	Change History is in CVS:
*/

#include <stdarg.h>
#include <sys/malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>

#include "LogMessage.h"
#include "webdavd.h"
#include "webdav_utils.h"

/*	-------------------------------------------------------------------------
        Debug printing defines
	------------------------------------------------------------------------- */
u_int32_t gPrintLevel = kNone | kSysLog | kAll;


void LogMessage(u_int32_t level, char *format, ...)
{
    va_list		arglist;
    int8_t		*buffer = NULL;
    int8_t		*bufPtr;

#if !DEBUG
	if (!(level & kSysLog)) {
		/* if we are not debugging, then only allow the syslog entries through */
		return;
	}
#endif
    if (level & gPrintLevel) {
		buffer = malloc (300);
		if (buffer == NULL)
			return;
		bzero (buffer, 300);
		
		bufPtr = buffer;

        va_start(arglist, format);
        vsnprintf ( (char*)buffer, 300, format, arglist);

        if (level & kSysLog) {
            syslog(LOG_DEBUG, "webdavfs: %s", buffer);
            level &= ~kSysLog;
        }

        if (level & gPrintLevel)
            syslog(LOG_DEBUG, "webdavfs: %s", buffer);
            //fprintf (stderr, (char*) "webdavfs: %s", buffer);
            
        va_end(arglist);
    }

    if (buffer != NULL) {
    	free(buffer);
        buffer = NULL;
    }
}

void logDebugCFString(const char *msg, CFStringRef str)
{
	char *cstr = NULL;
	
	if (str != NULL)
		cstr = createUTF8CStringFromCFString(str);
	
	if (msg == NULL) {
		if (cstr == NULL)
			return;	// nothing to do
		syslog(LOG_DEBUG, "%s\n", cstr);
	}
	else {
		if (cstr != NULL)
			syslog(LOG_DEBUG, "%s %s\n", msg, cstr);
		else
			syslog(LOG_DEBUG, "%s\n", msg);
	}

	if (cstr != NULL)
		free(cstr);
}

void logDebugCFURL(const char *msg, CFURLRef url)
{
	CFStringRef str = NULL;
	
	if (url != NULL) {
		str = CFURLGetString(url);
		CFRetain(str);
	}
	
	logDebugCFString(msg, str);
	
	if (str != NULL)
		CFRelease(str);
}

