/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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
#include "symbol_scope.h"

/**
 ** eapolclient logging
 **/

STATIC uint32_t	S_log_flags;

PRIVATE_EXTERN uint32_t
eapolclient_log_flags(void)
{
    return (S_log_flags);
}

PRIVATE_EXTERN void
eapolclient_log_set_flags(uint32_t log_flags, bool log_it)
{
    if (S_log_flags == log_flags) {
	return;
    }
    if (log_flags != 0) {
	EAPLogSetVerbose(TRUE);
	if (log_it) {
	    EAPLOG(LOG_NOTICE, "Verbose mode enabled (LogFlags = 0x%x)",
		   log_flags);
	}
    }
    else {
	if (log_it) {
	    EAPLOG(LOG_NOTICE, "Verbose mode disabled");
	}
	EAPLogSetVerbose(FALSE);
    }
    S_log_flags = log_flags;
    return;
}

PRIVATE_EXTERN bool
eapolclient_should_log(uint32_t flags)
{
    return ((S_log_flags & flags) != 0);
}
