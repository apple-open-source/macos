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

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"

static CFComparisonResult
sort_keys(const void *p1, const void *p2, void *context) {
	CFStringRef key1 = (CFStringRef)p1;
	CFStringRef key2 = (CFStringRef)p2;
	return CFStringCompare(key1, key2, 0);
}


SCDStatus
SCDNotifierList(SCDSessionRef session, int regexOptions, CFArrayRef *notifierKeys)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	CFIndex			keyCnt;
	void			**keyRefs;
	CFArrayRef		keys;
	CFMutableArrayRef	sortedKeys;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierList:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	if (regexOptions & kSCDRegexKey) {
		keyCnt  = CFSetGetCount(sessionPrivate->reKeys);
		keyRefs = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(sessionPrivate->reKeys, keyRefs);
		keys = CFArrayCreate(NULL, keyRefs, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, keyRefs);
	} else {
		keyCnt  = CFSetGetCount(sessionPrivate->keys);
		keyRefs = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(sessionPrivate->keys, keyRefs);
		keys = CFArrayCreate(NULL, keyRefs, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, keyRefs);
	}

	sortedKeys = CFArrayCreateMutableCopy(NULL, keyCnt, keys);
	CFRelease(keys);
	CFArraySortValues(sortedKeys, CFRangeMake(0, keyCnt), sort_keys, NULL);

	*notifierKeys = sortedKeys;
	return SCD_OK;
}
