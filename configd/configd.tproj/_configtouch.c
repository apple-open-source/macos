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
 * June 20, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

int
__SCDynamicStoreTouchValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	int				sc_status;
	Boolean				newValue	= FALSE;
	CFPropertyListRef		value;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreTouchValue:"));
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
	 * 2. Grab the current (or establish a new) store entry for this key.
	 */
	sc_status = __SCDynamicStoreCopyValue(store, key, &value);
	switch (sc_status) {
		case kSCStatusNoKey :
			/* store entry does not exist, create */
			value    = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
			newValue = TRUE;
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  new time stamp = %@"), value);
			break;

		case kSCStatusOK :
			/* store entry exists */
			if (CFGetTypeID(value) == CFDateGetTypeID()) {
				/* the value is a CFDate, update the time stamp */
				CFRelease(value);
				value = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
				newValue = TRUE;
				SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  new time stamp = %@"), value);
			} /* else, we'll just save the data (again) to bump the instance */
			break;

		default :
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreCopyValue(): %s"), SCErrorString(sc_status));
			goto done;
	}

	sc_status = __SCDynamicStoreSetValue(store, key, value);

	if (newValue) {
		CFRelease(value);
	}

    done :

	/*
	 * 8. Release our lock.
	 */
	__SCDynamicStoreUnlock(store, TRUE);

	return kSCStatusOK;
}


kern_return_t
_configtouch(mach_port_t 		server,
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

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Touch key in configuration database."));
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

	*sc_status = __SCDynamicStoreTouchValue(mySession->store, key);
	CFRelease(key);

	return KERN_SUCCESS;
}
