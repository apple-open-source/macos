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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * January 8, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

CFStringRef
SCDynamicStoreKeyCreateComputerName(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreate(allocator,
				       CFSTR("%@/%@"),
				       kSCDynamicStoreDomainSetup,
				       kSCCompSystem);
}


CFStringRef
SCDynamicStoreCopyComputerName(SCDynamicStoreRef	store,
			       CFStringEncoding		*nameEncoding)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		name		= NULL;
	SCDynamicStoreRef	mySession	= store;

	if (!store) {
		mySession = SCDynamicStoreCreate(NULL,
						 CFSTR("SCDynamicStoreCopyComputerName"),
						 NULL,
						 NULL);
		if (!mySession) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
	}

	key  = SCDynamicStoreKeyCreateComputerName(NULL);
	dict = SCDynamicStoreCopyValue(mySession, key);
	CFRelease(key);
	if (!dict) {
		goto done;
	}
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	name = isA_CFString(CFDictionaryGetValue(dict, kSCPropSystemComputerName));
	if (!name) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}
	CFRetain(name);

	if (nameEncoding) {
		CFNumberRef	num;

		num = CFDictionaryGetValue(dict,
					   kSCPropSystemComputerNameEncoding);
		if (isA_CFNumber(num)) {
			CFNumberGetValue(num, kCFNumberIntType, nameEncoding);
		} else {
			*nameEncoding = CFStringGetSystemEncoding();
		}
	}

    done :

	if (!store && mySession)	CFRelease(mySession);
	if (dict)			CFRelease(dict);
	return name;
}


Boolean
SCPreferencesSetComputerName(SCPreferencesRef	session,
			     CFStringRef	name,
			     CFStringEncoding	encoding)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict	= NULL;
	CFNumberRef		num;
	Boolean			ok	= FALSE;
	CFStringRef		path	= NULL;

	if (CFGetTypeID(name) != CFStringGetTypeID()) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefSystem,
					kSCCompSystem);

	dict = SCPreferencesPathGetValue(session, path);
	if (dict) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	CFDictionarySetValue(newDict, kSCPropSystemComputerName, name);

	num = CFNumberCreate(NULL, kCFNumberIntType, &encoding);
	CFDictionarySetValue(newDict, kSCPropSystemComputerNameEncoding, num);
	CFRelease(num);

	ok = SCPreferencesPathSetValue(session, path, newDict);
	if (!ok) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesPathSetValue() failed"));
	}

	if (path)	CFRelease(path);
	if (newDict)	CFRelease(newDict);

	return ok;
}
