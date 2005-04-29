/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
#include <SystemConfiguration/SCPrivate.h>
#include "mylog.h"

static bool S_verbose;

void
my_log_init(bool verbose)
{
    S_verbose = verbose;
    return;
}

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;
    char		buffer[256];

    if (priority == LOG_DEBUG) {
	if (S_verbose == FALSE)
	    return;
	priority = LOG_NOTICE;
    }
    va_start(ap, message);
    vsnprintf(buffer, sizeof(buffer), message, ap);
    SCLog(TRUE, priority, CFSTR("%s"), buffer);
    return;
}
