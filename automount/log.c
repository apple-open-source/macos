/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#if NS_TARGET_MAJOR == 3
#include <libc.h>
#endif
#include "log.h"

static char *msg_str = NULL;

#define LOGSTAMP_TIME 1
#define LOGSTAMP_HOSTNAME 0
#define LOGSTAMP_PROGRAMNAME 0
#define LOGSTAMP_PID 0
#define LOGSTAMP_LOGMSG 1

char *formattimevalue(struct nfstime *t, char *str, size_t stringlength) {
	if (stringlength < FORMATTED_TIME_LEN) return "";
	
	ctime_r((time_t *)&t->seconds, str);
	sprintf(&str[19], ".%06d", t->useconds);
	
	return str;
}

void
sys_openlog(char *str, int flags, int facility)
{
	if (msg_str != NULL) free(msg_str);
	msg_str = NULL;
	if (str != NULL)
	{
		msg_str = malloc(strlen(str) + 1);
		strcpy(msg_str, str);
	}

	openlog(msg_str, flags, facility);
}
	
#if LOGSTAMP_HOSTNAME
static int findhostname(char *hostname, size_t namelength, int shortenhostname) {
	int result;
	char *period;
	
	result = gethostname(hostname, namelength);
	if ((result == 0) && shortenhostname) {
		period = strchr(hostname, '.');
		if (period) *period = (char)0;
	}
	
	return result;
}
#endif

static void printlogentryheader(void) {
#if LOGSTAMP_TIME
	struct timeval currenttime;
	char datetime[26];
#endif
#if LOGSTAMP_HOSTNAME
	char hostname[128];
#endif
	
#if LOGSTAMP_TIME
	/* The format of ctime_r()'s output is a fixed, 26-character layout: day mmm dd hh:mm:ss yyyy\n\0 */
	gettimeofday(&currenttime, NULL);
	ctime_r((time_t *)&currenttime.tv_sec, datetime);
	/* datetime[19] is just after the "hh:mm:ss" field. */
	sprintf(&datetime[19], ".%03d", currenttime.tv_usec / 1000);
	datetime[23] = (char)0;
	fprintf(stderr, "%s", datetime);
#endif

#if LOGSTAMP_HOSTNAME
	findhostname(hostname, sizeof(hostname), 1);
	if (LOGSTAMP_TIME) fprintf(stderr, " ");
	fprintf(stderr, "%s", hostname);
#endif

#if LOGSTAMP_PROGRAMNAME
	if (LOGSTAMP_TIME || LOGSTAMP_HOSTNAME) fprintf(stderr, " ");
	fprintf(stderr, "%s", getprogname());
#endif

#if LOGSTAMP_PID
	if (!LOGSTAMP_PROGRAMNAME) fprintf(stderr, " ");
	fprintf(stderr, "[%u]", getpid());
#endif

	if (LOGSTAMP_TIME || LOGSTAMP_HOSTNAME || LOGSTAMP_PROGRAMNAME || LOGSTAMP_PID) {
		if (!LOGSTAMP_LOGMSG) fprintf(stderr, ":");
		fprintf(stderr, " ");
	}
}

void
sys_msg(int debug, int priority, char *message, ...)
{
	va_list ap;

	if (debug == 0) return;

	va_start(ap, message);

	if (debug & DEBUG_SYSLOG)
		vsyslog(priority, message, ap);

	if (debug & DEBUG_STDERR)
	{
		printlogentryheader();
#if LOGSTAMP_LOGMSG
		if (msg_str != NULL) fprintf(stderr, "%s[%u]: ", msg_str, getpid());
#endif
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
		fflush(stderr);
	}

	va_end(ap);
}
	
