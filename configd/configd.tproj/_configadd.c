/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

int
__SCDynamicStoreAddValue(SCDynamicStoreRef store, CFStringRef key, CFPropertyListRef value)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFPropertyListRef		tempValue;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreAddValue:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  value        = %@"), value);

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
	 * 2. Ensure that this is a new key.
	 */
	sc_status = __SCDynamicStoreCopyValue(store, key, &tempValue);
	switch (sc_status) {
		case kSCStatusNoKey :
			/* store key does not exist, proceed */
			break;

		case kSCStatusOK :
			/* store key exists, sorry */
			CFRelease(tempValue);
			sc_status = kSCStatusKeyExists;
			goto done;

		default :
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  _SCDGet(): %s"), SCErrorString(sc_status));
			goto done;
	}

	/*
	 * 3. Save the new key.
	 */
	sc_status = __SCDynamicStoreSetValue(store, key, value);

	/*
	 * 4. Release our lock.
	 */
    done:
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}


kern_return_t
_configadd(mach_port_t 			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   xmlData_t			dataRef,	/* raw XML bytes */
	   mach_msg_type_number_t	dataLen,
	   int				*newInstance,
	   int				*sc_status
)
{
	serverSessionRef	mySession = getSession(server);
	CFStringRef		key;		/* key  (un-serialized) */
	CFPropertyListRef	data;		/* data (un-serialized) */

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Add key to configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*sc_status = kSCStatusOK;

	/* un-serialize the key */
	if (!_SCUnserialize((CFPropertyListRef *)&key, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
	}

	if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	/* un-serialize the data */
	if (!_SCUnserialize((CFPropertyListRef *)&data, (void *)dataRef, dataLen)) {
		*sc_status = kSCStatusFailed;
	}

	if (!isA_CFPropertyList(data)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	if (*sc_status != kSCStatusOK) {
		if (key)	CFRelease(key);
		if (data)	CFRelease(data);
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreAddValue(mySession->store, key, data);
	*newInstance = 0;

	CFRelease(key);
	CFRelease(data);

	return KERN_SUCCESS;
}
