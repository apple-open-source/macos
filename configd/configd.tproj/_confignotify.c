/*
 * Copyright (c) 2000-2004, 2006, 2008 Apple Inc. All rights reserved.
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
 * May 19, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"

__private_extern__
int
__SCDynamicStoreNotifyValue(SCDynamicStoreRef store, CFStringRef key, Boolean internal)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFDictionaryRef			dict;
	Boolean				newValue	= FALSE;
	CFDataRef			value;

	if ((store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("%s : %5d : %@\n"),
			internal ? "*notify" : "notify ",
			storePrivate->server,
			key);
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
	if (!dict || !CFDictionaryGetValueIfPresent(dict, kSCDData, (const void **)&value)) {
		/* key doesn't exist (or data never defined) */
		(void)_SCSerialize(kCFBooleanTrue, &value, NULL, NULL);
		newValue = TRUE;
	}

	/* replace or store initial/temporary existing value */
	__SCDynamicStoreSetValue(store, key, value, TRUE);

	if (newValue) {
		/* remove the value we just created */
		__SCDynamicStoreRemoveValue(store, key, TRUE);
		CFRelease(value);
	}

	/*
	 * 3. Release our lock.
	 */
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}


__private_extern__
kern_return_t
_confignotify(mach_port_t 		server,
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

	*sc_status = __SCDynamicStoreNotifyValue(mySession->store, key, FALSE);

    done :

	if (key)	CFRelease(key);
	return KERN_SUCCESS;
}
