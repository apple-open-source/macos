/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

__private_extern__
int
__SCDynamicStoreAddValue(SCDynamicStoreRef store, CFStringRef key, CFDataRef value)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFDataRef			tempValue;

	if (_configd_verbose) {
		CFPropertyListRef	val;

		(void) _SCUnserialize(&val, value, NULL, NULL);
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreAddValue:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  key          = %@"), key);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  value        = %@"), val);
		CFRelease(val);
	}

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace, CFSTR("add     : %5d : %@\n"), storePrivate->server, key);
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
	sc_status = __SCDynamicStoreCopyValue(store, key, &tempValue, TRUE);
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
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreCopyValue(): %s"), SCErrorString(sc_status));
			goto done;
	}

	/*
	 * 3. Save the new key.
	 */
	sc_status = __SCDynamicStoreSetValue(store, key, value, TRUE);

	/*
	 * 4. Release our lock.
	 */
    done:
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}


__private_extern__
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
	CFDataRef		data;		/* data (un-serialized) */

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Add key to configuration database."));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  server = %d"), server);
	}

	*sc_status = kSCStatusOK;

	/* un-serialize the key */
	if (!_SCUnserializeString(&key, NULL, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
	} else if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	/* un-serialize the data */
	if (!_SCUnserializeData(&data, (void *)dataRef, dataLen)) {
		*sc_status = kSCStatusFailed;
	}

	if (!mySession) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
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
