/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * Nov 28, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

CFStringRef
SCDynamicStoreKeyCreateLocation(CFAllocatorRef allocator)
{
	return CFRetain(kSCDynamicStoreDomainSetup);
}


CFStringRef
SCDynamicStoreCopyLocation(SCDynamicStoreRef store)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		location	= NULL;
	Boolean			tempSession	= FALSE;

	if (!store) {
		store = SCDynamicStoreCreate(NULL,
					     CFSTR("SCDynamicStoreCopyLocation"),
					     NULL,
					     NULL);
		if (!store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
		tempSession = TRUE;
	}

	key  = SCDynamicStoreKeyCreateLocation(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	location = CFDictionaryGetValue(dict, kSCDynamicStorePropSetupCurrentSet);
	if (!isA_CFString(location)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	CFRetain(location);

    done :

	if (tempSession)	CFRelease(store);
	if (dict)		CFRelease(dict);

	return location;
}
