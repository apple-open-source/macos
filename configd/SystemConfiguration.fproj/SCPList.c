/*
 * Copyright(c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1(the
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

#include <SystemConfiguration/SCP.h>
#include "SCPPrivate.h"

#include <SystemConfiguration/SCD.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>


static CFComparisonResult
sort_keys(const void *p1, const void *p2, void *context) {
	CFStringRef key1 = (CFStringRef)p1;
	CFStringRef key2 = (CFStringRef)p2;
	return CFStringCompare(key1, key2, 0);
}


SCPStatus
SCPList(SCPSessionRef session, CFArrayRef *keys)
{
	SCPSessionPrivateRef	sessionPrivate;
	CFIndex			prefsCnt;
	void			**prefsKeys;
	CFArrayRef		allKeys;
	CFMutableArrayRef	sortedKeys;

	if (session == NULL) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)session;

	prefsCnt  = CFDictionaryGetCount(sessionPrivate->prefs);
	prefsKeys = CFAllocatorAllocate(NULL, prefsCnt * sizeof(CFStringRef), 0);
	CFDictionaryGetKeysAndValues(sessionPrivate->prefs, prefsKeys, NULL);
	allKeys = CFArrayCreate(NULL, prefsKeys, prefsCnt, &kCFTypeArrayCallBacks);
	CFAllocatorDeallocate(NULL, prefsKeys);

	sortedKeys = CFArrayCreateMutableCopy(NULL, prefsCnt, allKeys);
	CFRelease(allKeys);
	CFArraySortValues(sortedKeys,
			  CFRangeMake(0, prefsCnt),
			  sort_keys,
			  NULL);

	*keys = sortedKeys;
	return SCP_OK;
}
