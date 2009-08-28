/*
 * Copyright (c) 2001-2009 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <CoreFoundation/CoreFoundation.h>
#include "mylog.h"

static bool S_verbose;

void
my_log_set_verbose(bool verbose)
{
    S_verbose = verbose;
    return;
}

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (S_verbose == FALSE)
	    return;
	priority = LOG_NOTICE;
    }

    va_start(ap, message);
    vsyslog(priority, message, ap);
    va_end(ap);

    return;
}

static void
fprint_time(FILE * f)
{
    time_t		t;
    struct tm		tm;
    struct timeval	tv;

    (void)gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    (void)localtime_r(&t, &tm);

    fprintf(f, "%04d/%02d/%02d %2d:%02d:%02d.%06d ",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec,
	    tv.tv_usec);
    return;
}

void
timestamp_fprintf(FILE * f, const char * message, ...)
{
    va_list		ap;

    fprint_time(f);
    va_start(ap, message);
    vfprintf(f, message, ap);
    va_end(ap);
}

/**
 ** eapolclient logging
 **/

static FILE * 	S_log_file;
static uint32_t	S_log_flags;

void
eapolclient_log_set(FILE * log_file, uint32_t log_flags)
{
    S_log_file = log_file;
    if (S_log_file != NULL) {
	S_log_flags = log_flags;
    }
    else {
	S_log_flags = 0;
    }
    return;
}

bool
eapolclient_should_log(uint32_t flags)
{
    bool	should_log;

    if (S_log_file == NULL || (S_log_flags & flags) == 0) {
	should_log = FALSE;
    }
    else {
	should_log = TRUE;
    }
    return (should_log);
}

static void
fwrite_plist(FILE * f, CFPropertyListRef p)
{
    CFDataRef	data;

    data = CFPropertyListCreateXMLData(NULL, p);
    if (data == NULL) {
	return;
    }
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, f);
    CFRelease(data);
    return;
}

void
eapolclient_log(uint32_t flags, const char * message, ...)
{
    va_list		ap;

    if ((S_log_flags & kLogFlagIncludeStdoutStderr) != 0) {
	fflush(stdout);
	fflush(stderr);
    }
    if (eapolclient_should_log(flags) == FALSE) {
	return;
    }
    va_start(ap, message);
    fprint_time(S_log_file);
    vfprintf(S_log_file, message, ap);
    va_end(ap);
    fflush(S_log_file);
    return;
}

void
eapolclient_log_plist(uint32_t flags, CFPropertyListRef plist)
{
    if ((S_log_flags & kLogFlagIncludeStdoutStderr) != 0) {
	fflush(stdout);
	fflush(stderr);
    }
    if (eapolclient_should_log(flags) == FALSE) {
	return;
    }
    fwrite_plist(S_log_file, plist);
    fflush(S_log_file);
    return;
}

FILE *
eapolclient_log_file(void)
{
    return (S_log_file);
}
