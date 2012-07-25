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
    u_int8_t* 	findex;
    int8_t  	c;
    int32_t		intarg;
    u_int32_t		uintarg;
    int8_t*		strarg;
//    float		floatarg;
	u_int64_t   ulonglongarg;
    char		chararg;
    void*		pointerarg;
    va_list		arglist;
    int8_t		*buffer = NULL;
    int8_t		*bufPtr;
    int		len = 0;

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

        findex = (u_int8_t*) format;        
        while (*findex != 0) {
            c = *findex;
            if (c == '%') {
                ++findex;
                c = *findex;
                switch (c) {
                    case 'i':
                    case 'd':
                        intarg = va_arg(arglist, int32_t);
                        len += snprintf((char*) bufPtr, 300 - len, "%d", intarg);
                        bufPtr += len;
                        break;
                    case 'X':
                    case 'x':
                        uintarg = va_arg(arglist, u_int32_t);
                        len += snprintf((char*) bufPtr, 300 - len, "%X", uintarg);
                        bufPtr += len;
                        break;
                    case 'p':
                        pointerarg = va_arg(arglist, void*);
                        len += snprintf((char*) bufPtr, 300 - len, "%p", pointerarg);
                        bufPtr += len;
                        break;
                    case 'f':
                        ulonglongarg = va_arg(arglist, u_int64_t);
                        len += snprintf((char*) bufPtr, 300 - len, "%llx", (u_int64_t) ulonglongarg);
                        bufPtr += len;
                        break;
                    case 's':
                        strarg = va_arg(arglist, int8_t*);
                        if (strarg != NULL) 
                                len += snprintf((char*) bufPtr, 300 - len, "%s", strarg);
                        else
                                len += snprintf((char*) bufPtr, 300 - len, "NULLSTR");
                        bufPtr += len;
                        break;
                    case 'c':
                        chararg = va_arg(arglist, int);
                        len += snprintf((char*) bufPtr, 300 - len, "%c", chararg);
                        bufPtr += len;
                        break;
                    default:
                        len += snprintf((char*) bufPtr, 300 - len, "%c", c);
                        bufPtr += len;
                        break;
                }
            }
            else {
                len += snprintf((char*) bufPtr, 300 - len, "%c", c);
                bufPtr += len;
            }
            ++findex;
        }

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

