/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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

/*
 * mylog.c
 * - logging related functions
 */

/* 
 * Modification History
 *
 * June 15, 2009		Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#include "mylog.h"
#include <stdlib.h>
#include <sys/time.h>
#include <stdarg.h>
#include "globals.h"

/* 
 * Function: timestamp_fprintf
 *
 * Purpose:
 *   Print a timestamped message.
 */
void
timestamp_fprintf(FILE * f, const char * message, ...)
{
    struct timeval	tv;
    struct tm       	tm;
    time_t		t;
    va_list		ap;

    (void)gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    (void)localtime_r(&t, &tm);

    va_start(ap, message);
    fprintf(f, "%04d/%02d/%02d %2d:%02d:%02d.%06d ",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec,
	    tv.tv_usec);
    vfprintf(f, message, ap);
    va_end(ap);
}

void
my_log(int priority, const char * message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (G_IPConfiguration_verbose == 0) {
	    return;
	}
	priority = LOG_NOTICE;
    }
    else if (priority == LOG_INFO) {
	priority = LOG_NOTICE;
    }
    va_start(ap, message);
    vsyslog(priority, message, ap);
    va_end(ap);
    return;
}
