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
		if (msg_str != NULL) fprintf(stderr, "%s[%u]: ", msg_str, getpid());
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
		fflush(stderr);
	}

	va_end(ap);
}
	
