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
 * May 19, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

int
__SCDynamicStoreNotifyValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFDictionaryRef			dict;
	Boolean				newValue	= FALSE;
	CFPropertyListRef		value;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreNotifyValue:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key = %@"), key);

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	/*
	 * 1. Ensure that we hold the lock.
	 */
	sc_status = __SCDynamicStoreLock(store, TRUE);
	if (sc_status != kSCStatusOK) {
		return sc_status;
	}

	/*
	 * 2. Tickle the value in the dynamic store
	 */
	dict = CFDictionaryGetValue(storeData, key);
	if (!dict || !CFDictionaryGetValueIfPresent(dict, kSCDData, (void **)&value)) {
		/* key doesn't exist (or data never defined) */
		value = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
		newValue = TRUE;
	}

	/* replace or store initial/temporary existing value */
	__SCDynamicStoreSetValue(store, key, value);

	if (newValue) {
		/* remove the value we just created */
		__SCDynamicStoreRemoveValue(store, key);
		CFRelease(value);
	}

	/*
	 * 3. Release our lock.
	 */
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}


kern_return_t
_confignotify(mach_port_t 		server,
	      xmlData_t			keyRef,		/* raw XML bytes */
	      mach_msg_type_number_t	keyLen,
	      int			*sc_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFStringRef		xmlError;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Notify key in configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

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
		return KERN_SUCCESS;
	} else if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreNotifyValue(mySession->store, key);
	CFRelease(key);

	return KERN_SUCCESS;
}
