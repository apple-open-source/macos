/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Modification History
 *
 * May 18, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

CFStringRef
SCDynamicStoreKeyCreateProxies(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreateNetworkGlobalEntity(allocator,
							  kSCDynamicStoreDomainState,
							  kSCEntNetProxies);
}


CFDictionaryRef
SCDynamicStoreCopyProxies(SCDynamicStoreRef store)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFDictionaryRef		proxies		= NULL;
	Boolean			tempSession	= FALSE;

	if (!store) {
		store = SCDynamicStoreCreate(NULL,
					     CFSTR("SCDynamicStoreCopyProxies"),
					     NULL,
					     NULL);
		if (!store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
		tempSession = TRUE;
	}

	key  = SCDynamicStoreKeyCreateProxies(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	proxies = CFRetain(dict);

    done :

	if (tempSession)	CFRelease(store);
	if (dict)		CFRelease(dict);
	return proxies;
}
