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

#import <syslog.h>
#import <stdarg.h>
#import <syslog.h>

#import <mach/boolean.h>
#import "lockc.h"

static lockc_t		log;
static int		verbose = FALSE;
static int		initialized = FALSE;

void
ts_log_init(int v)
{
    verbose = v;
    lockc_init(&log);
    initialized = TRUE;
}

void
ts_log(int priority, const char *message, ...)
{
    va_list 		ap;
    static boolean_t	log_busy = FALSE;

    if (initialized == FALSE)
	return;

    if (priority == LOG_DEBUG) {
	if (verbose == FALSE)
	    return;
	priority = LOG_INFO;
    }

    va_start(ap, message);

    lockc_lock(&log);
    while (log_busy)
	 lockc_wait(&log);
    log_busy = TRUE;
    lockc_unlock(&log);
    vsyslog(priority, message, ap);
    lockc_lock(&log);
    log_busy = FALSE;
    lockc_signal_nl(&log);
    lockc_unlock(&log);
    return;
}

