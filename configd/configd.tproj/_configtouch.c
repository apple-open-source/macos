/*
 * Copyright (c) 2000-2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
 * June 20, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

__private_extern__
int
__SCDynamicStoreTouchValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFDataRef			value;

	if ((store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace, CFSTR("touch   : %5d : %@\n"), storePrivate->server, key);
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
	sc_status = __SCDynamicStoreCopyValue(store, key, &value, TRUE);
	switch (sc_status) {
		case kSCStatusNoKey : {
			CFDateRef	now;

			/* store entry does not exist, create */

			now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
			(void) _SCSerialize(now, &value, NULL, NULL);
			CFRelease(now);
			break;
		}

		case kSCStatusOK : {
			CFDateRef	now;

			/* store entry exists */

			(void) _SCUnserialize((CFPropertyListRef *)&now, value, NULL, 0);
			if (isA_CFDate(now)) {
				/* the value is a CFDate, update the time stamp */
				CFRelease(now);
				CFRelease(value);
				now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
				(void) _SCSerialize(now, &value, NULL, NULL);
			} /* else, we'll just save the data (again) to bump the instance */
			CFRelease(now);

			break;
		}
		default :
			goto done;
	}

	sc_status = __SCDynamicStoreSetValue(store, key, value, TRUE);
	CFRelease(value);

    done :

	/*
	 * 8. Release our lock.
	 */
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}


__private_extern__
kern_return_t
_configtouch(mach_port_t 		server,
	     xmlData_t			keyRef,		/* raw XML bytes */
	     mach_msg_type_number_t	keyLen,
	     int			*sc_status
)
{
	CFStringRef		key		= NULL;		/* key  (un-serialized) */
	serverSessionRef	mySession;

	/* un-serialize the key */
	if (!_SCUnserializeString(&key, NULL, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
		goto done;
	}

	if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	mySession = getSession(server);
	if (mySession == NULL) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		goto done;
	}

	*sc_status = __SCDynamicStoreTouchValue(mySession->store, key);

    done :

	if (key)	CFRelease(key);
	return KERN_SUCCESS;
}
