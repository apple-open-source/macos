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
 * January 2, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

CFStringRef
SCDynamicStoreKeyCreateConsoleUser(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreate(allocator,
				       CFSTR("%@/%@/%@"),
				       kSCDynamicStoreDomainState,
				       kSCCompUsers,
				       kSCEntUsersConsoleUser);
}


CFStringRef
SCDynamicStoreCopyConsoleUser(SCDynamicStoreRef	store,
			      uid_t		*uid,
			      gid_t		*gid)
{
	CFStringRef		consoleUser	= NULL;
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	SCDynamicStoreRef	mySession	= store;

	if (!store) {
		mySession = SCDynamicStoreCreate(NULL,
						 CFSTR("SCDynamicStoreCopyConsoleUser"),
						 NULL,
						 NULL);
		if (!mySession) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
	}

	key  = SCDynamicStoreKeyCreateConsoleUser(NULL);
	dict = SCDynamicStoreCopyValue(mySession, key);
	CFRelease(key);
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	consoleUser = CFDictionaryGetValue(dict, kSCPropUsersConsoleUserName);
	consoleUser = isA_CFString(consoleUser);
	if (!consoleUser) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	CFRetain(consoleUser);

	if (uid) {
		CFNumberRef	num;
		SInt32		val;

		num = CFDictionaryGetValue(dict, kSCPropUsersConsoleUserUID);
		if (isA_CFNumber(num)) {
			if (CFNumberGetValue(num, kCFNumberSInt32Type, &val)) {
				*uid = (uid_t)val;
			}
		}
	}

	if (gid) {
		CFNumberRef	num;
		SInt32		val;

		num = CFDictionaryGetValue(dict, kSCPropUsersConsoleUserGID);
		if (isA_CFNumber(num)) {
			if (CFNumberGetValue(num, kCFNumberSInt32Type, &val)) {
				*gid = (gid_t)val;
			}
		}
	}

    done :

	if (!store && mySession)	CFRelease(mySession);
	if (dict)			CFRelease(dict);
	return consoleUser;

}


Boolean
SCDynamicStoreSetConsoleUser(SCDynamicStoreRef	store,
			     const char		*user,
			     uid_t		uid,
			     gid_t		gid)
{
	CFStringRef		consoleUser;
	CFMutableDictionaryRef	dict		= NULL;
	CFStringRef		key		= SCDynamicStoreKeyCreateConsoleUser(NULL);
	SCDynamicStoreRef	mySession	= store;
	CFNumberRef		num;
	Boolean			ok		= TRUE;

	if (!store) {
		mySession = SCDynamicStoreCreate(NULL,
						 CFSTR("SCDynamicStoreSetConsoleUser"),
						 NULL,
						 NULL);
		if (!mySession) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return FALSE;
		}
	}

	if (user == NULL) {
		ok = SCDynamicStoreRemoveValue(mySession, key);
		goto done;
	}

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

	consoleUser = CFStringCreateWithCString(NULL, user, kCFStringEncodingMacRoman);
	CFDictionarySetValue(dict, kSCPropUsersConsoleUserName, consoleUser);
	CFRelease(consoleUser);

	num = CFNumberCreate(NULL, kCFNumberSInt32Type, (SInt32 *)&uid);
	CFDictionarySetValue(dict, kSCPropUsersConsoleUserUID, num);
	CFRelease(num);

	num = CFNumberCreate(NULL, kCFNumberSInt32Type, (SInt32 *)&gid);
	CFDictionarySetValue(dict, kSCPropUsersConsoleUserGID, num);
	CFRelease(num);

	ok = SCDynamicStoreSetValue(mySession, key, dict);

    done :

	if (dict)			CFRelease(dict);
	if (key)			CFRelease(key);
	if (!store && mySession)	CFRelease(mySession);
	return ok;
}
