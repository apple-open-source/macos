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
 * October 17, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

int
__SCDynamicStoreAddTemporaryValue(SCDynamicStoreRef store, CFStringRef key, CFPropertyListRef value)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status = kSCStatusOK;
	CFStringRef			sessionKey;
	CFDictionaryRef			dict;
	CFMutableDictionaryRef		newDict;
	CFArrayRef			keys;
	CFMutableArrayRef		newKeys;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreAddTemporaryValue:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  value        = %@"), value);

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	/*
	 * 1. Add the key
	 */
	sc_status = __SCDynamicStoreAddValue(store, key, value);
	if (sc_status != kSCStatusOK) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreAddValue(): %s"), SCErrorString(sc_status));
		return sc_status;
	}

	/*
	 * 2. Create the session key
	 */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);

	/*
	 * 3. Add this key to my list of per-session keys
	 */
	dict = CFDictionaryGetValue(sessionData, sessionKey);
	keys = CFDictionaryGetValue(dict, kSCDSessionKeys);
	if ((keys == NULL) ||
	    (CFArrayGetFirstIndexOfValue(keys,
					 CFRangeMake(0, CFArrayGetCount(keys)),
					 key) == -1)) {
		/*
		 * if no session keys defined "or" keys defined but not
		 * this one...
		 */
		if (keys) {
			/* this is a new session key */
			newKeys = CFArrayCreateMutableCopy(NULL, 0, keys);
		} else {
			/* this is an additional session key */
			newKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(newKeys, key);

		/* update session dictionary */
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFDictionarySetValue(newDict, kSCDSessionKeys, newKeys);
		CFRelease(newKeys);
		CFDictionarySetValue(sessionData, sessionKey, newDict);
		CFRelease(newDict);
	}

	/*
	 * 4. Mark the key as a "session" key and track the creator.
	 */
	dict    = CFDictionaryGetValue(storeData, key);
	newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFDictionarySetValue(newDict, kSCDSession, sessionKey);
	CFDictionarySetValue(storeData, key, newDict);
	CFRelease(newDict);

	CFRelease(sessionKey);
	return sc_status;
}


kern_return_t
_configadd_s(mach_port_t 		server,
	     xmlData_t			keyRef,		/* raw XML bytes */
	     mach_msg_type_number_t	keyLen,
	     xmlData_t			dataRef,	/* raw XML bytes */
	     mach_msg_type_number_t	dataLen,
	     int			*newInstance,
	     int			*sc_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		xmlData;	/* data (XML serialized) */
	CFPropertyListRef	data;		/* data (un-serialized) */
	CFStringRef		xmlError;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Add (session) key to configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*sc_status = kSCStatusOK;

	/* un-serialize the key */
	xmlKey = CFDataCreate(NULL, keyRef, keyLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)keyRef, keyLen);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	key = CFPropertyListCreateFromXMLData(NULL,
					      xmlKey,
					      kCFPropertyListImmutable,
					      &xmlError);
	CFRelease(xmlKey);
	if (!key) {
		if (xmlError) {
			SCLog(_configd_verbose, LOG_DEBUG,
			       CFSTR("CFPropertyListCreateFromXMLData() key: %@"),
			       xmlError);
			CFRelease(xmlError);
		}
		*sc_status = kSCStatusFailed;
	} else if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	/* un-serialize the data */
	xmlData = CFDataCreate(NULL, dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	data = CFPropertyListCreateFromXMLData(NULL,
					       xmlData,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlData);
	if (!data) {
		if (xmlError) {
			SCLog(_configd_verbose, LOG_DEBUG,
			       CFSTR("CFPropertyListCreateFromXMLData() data: %@"),
			       xmlError);
			CFRelease(xmlError);
		}
		*sc_status = kSCStatusFailed;
	} else if (!isA_CFPropertyList(data)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	if (*sc_status != kSCStatusOK) {
		if (key)	CFRelease(key);
		if (data)	CFRelease(data);
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreAddTemporaryValue(mySession->store, key, data);
	if (*sc_status == kSCStatusOK) {
		*newInstance = 1;
	}
	CFRelease(key);
	CFRelease(data);

	return KERN_SUCCESS;
}
