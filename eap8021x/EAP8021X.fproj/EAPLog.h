/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
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
 * EAPLog.h
 * - function to log information for EAP-related routines
 */

/* 
 * Modification History
 *
 * December 26, 2012	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _EAP8021X_EAPLOG_H
#define _EAP8021X_EAPLOG_H

#include <SystemConfiguration/SCPrivate.h>
#include "symbol_scope.h"

void
EAPLogSetVerbose(bool verbose);

void
EAPLog(int level, CFStringRef format, ...);

INLINE const char *
EAPLogFileName(const char * file)
{
    const char *	ret;

    ret = strrchr(file, '/');
    if (ret != NULL) {
	ret++;
    }
    else {
	ret = file;
    }
    return (ret);
}

#define EAPLOG(__level, __format, ...)				\
    EAPLog(__level, CFSTR(__format),				\
	   ## __VA_ARGS__)

#define EAPLOG_FL(__level, __format, ...)				\
    EAPLog(__level,							\
	   CFSTR("[%s:%d] %s(): " __format),				\
	   EAPLogFileName(__FILE__), __LINE__, __FUNCTION__,		\
	   ## __VA_ARGS__)

#endif /* _EAP8021X_EAPLOG_H */
