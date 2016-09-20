/*
 * Copyright (c) 2012-2016 Apple Inc. All rights reserved.
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

typedef CF_ENUM(uint32_t, EAPLogCategory) {
	kEAPLogCategoryController	= 0,
	kEAPLogCategoryMonitor		= 1,
	kEAPLogCategoryClient		= 2,
	kEAPLogCategoryFramework	= 3,
};

os_log_t
EAPLogGetLogHandle();

void
EAPLogInit(EAPLogCategory log_category);

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

#define EAPLOG(level, format, ...)											\
    do {																	\
		os_log_t log_handle = EAPLogGetLogHandle();					\
		os_log_type_t __type = _SC_syslog_os_log_mapping(level);			\
		os_log_with_type(log_handle, __type, format, ## __VA_ARGS__);		\
	} while (0)

#define EAPLOG_FL EAPLOG

#endif /* _EAP8021X_EAPLOG_H */
