/*
	File:		LogMessage.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 2001-2006 by Apple Computer, Inc., all rights reserved.

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
    int8_t		tempStr[80];

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
                        bufPtr += sprintf((char*) bufPtr, "%d", intarg);
                        break;
                    case 'X':
                    case 'x':
                        uintarg = va_arg(arglist, u_int32_t);
                        bufPtr += sprintf((char*) bufPtr, "%X", uintarg);
                        break;
                    case 'p':
                        pointerarg = va_arg(arglist, void*);
                        bufPtr += sprintf((char*) bufPtr, "%p", pointerarg);
                        break;
                    case 'f':
                        ulonglongarg = va_arg(arglist, u_int64_t);
                        bufPtr += sprintf((char*) bufPtr, "%llx", (u_int64_t) ulonglongarg);
                        break;
                    case 's':
                        strarg = va_arg(arglist, int8_t*);
                        if (strarg != NULL) {
                            if (strlen ((char*) strarg) > sizeof (tempStr)) {
                                /* strarg is too long and could overrun my print buffer, so truncate it down */
                                strncpy ((char*) tempStr, (char*) strarg, sizeof(tempStr));
                                bufPtr += sprintf((char*) bufPtr, "%s...", tempStr);
                            }
                            else
                                bufPtr += sprintf((char*) bufPtr, "%s", strarg);
                        }
                        else
                                bufPtr += sprintf((char*) bufPtr, "NULLSTR");
                        break;
                    case 'c':
                        chararg = va_arg(arglist, int);
                        bufPtr += sprintf((char*) bufPtr, "%c", chararg);
                        break;
                    default:
                        bufPtr += sprintf((char*) bufPtr, "%c", c);
                        break;
                }
            }
            else {
                bufPtr += sprintf((char*) bufPtr, "%c", c);
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

