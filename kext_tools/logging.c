/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include <libc.h>
#include <syslog.h>
#include "logging.h"
#include "globals.h"

static bool use_syslog = false;

/*******************************************************************************
*
*******************************************************************************/
void kextd_openlog(const char * name)
{
    openlog(name, LOG_CONS | LOG_NDELAY | LOG_PID,
        LOG_DAEMON);

    use_syslog = true;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_log(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);

    if (use_syslog) {
        vsyslog(LOG_NOTICE, format, ap);    // LOG_INFO disabled as of 10.4
    } else {
	char *newfmt;
	asprintf(&newfmt, "%s: %s\n", getprogname(), format);
	if (newfmt) {
	    vfprintf(stdout, newfmt, ap);
	    free(newfmt);
	} else {
	    vfprintf(stdout, format, ap);
	}
    }

    va_end(ap);
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_error_log(const char * format, ...)
{
    va_list ap;


    va_start(ap, format);

    if (use_syslog) {
        vsyslog(LOG_ERR, format, ap);
    } else {
	char *newfmt;
	asprintf(&newfmt, "%s: %s\n", getprogname(), format);
	if (newfmt) {
	    vfprintf(stderr, newfmt, ap);
	    free(newfmt);
	} else {
	    vfprintf(stderr, format, ap);
	}
    }

    va_end(ap);

}
