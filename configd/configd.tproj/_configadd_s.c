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

#include "configd.h"
#include "session.h"

SCDStatus
_SCDAddSession(SCDSessionRef session, CFStringRef key, SCDHandleRef handle)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	SCDStatus		scd_status = SCD_OK;
	CFStringRef		sessionKey;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFArrayRef		keys;
	CFMutableArrayRef	newKeys;

	SCDLog(LOG_DEBUG, CFSTR("_SCDAddSession:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  data         = %@"), SCDHandleGetData(handle));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/*
	 * 1. Add the key
	 */
	scd_status = _SCDAdd(session, key, handle);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("  _SCDAdd(): %s"), SCDError(scd_status));
		return scd_status;
	}

	/*
	 * 2. Create the session key
	 */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);

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
	dict    = CFDictionaryGetValue(cacheData, key);
	newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFDictionarySetValue(newDict, kSCDSession, sessionKey);
	CFDictionarySetValue(cacheData, key, newDict);
	CFRelease(newDict);

	CFRelease(sessionKey);
	return scd_status;
}


kern_return_t
_configadd_s(mach_port_t 		server,
	     xmlData_t			keyRef,		/* raw XML bytes */
	     mach_msg_type_number_t	keyLen,
	     xmlData_t			dataRef,	/* raw XML bytes */
	     mach_msg_type_number_t	dataLen,
	     int			*newInstance,
	     int			*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		xmlData;	/* data (XML serialized) */
	CFPropertyListRef	data;		/* data (un-serialized) */
	SCDHandleRef		handle;
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("Add (session) key to configuration database."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	/* un-serialize the key */
	xmlKey = CFDataCreate(NULL, keyRef, keyLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)keyRef, keyLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	key = CFPropertyListCreateFromXMLData(NULL,
					      xmlKey,
					      kCFPropertyListImmutable,
					      &xmlError);
	CFRelease(xmlKey);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() key: %s"), xmlError);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	/* un-serialize the data */
	xmlData = CFDataCreate(NULL, dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	data = CFPropertyListCreateFromXMLData(NULL,
					       xmlData,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlData);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() data: %s"), xmlError);
		CFRelease(key);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	handle = SCDHandleInit();
	SCDHandleSetData(handle, data);
	*scd_status = _SCDAddSession(mySession->session, key, handle);
	if (*scd_status == SCD_OK) {
		*newInstance = SCDHandleGetInstance(handle);
	}
	SCDHandleRelease(handle);
	CFRelease(key);
	CFRelease(data);

	return KERN_SUCCESS;
}
