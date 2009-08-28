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
__SCDynamicStoreRemoveValue(SCDynamicStoreRef store, CFStringRef key, Boolean internal)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status = kSCStatusOK;
	CFDictionaryRef			dict;
	CFMutableDictionaryRef		newDict;
	CFStringRef			sessionKey;

	if ((store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("%s : %5d : %@\n"),
			internal ? "*remove" : "remove ",
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
	 * 2. Ensure that this key exists.
	 */
	dict = CFDictionaryGetValue(storeData, key);
	if ((dict == NULL) || (CFDictionaryContainsKey(dict, kSCDData) == FALSE)) {
		/* key doesn't exist (or data never defined) */
		sc_status = kSCStatusNoKey;
		goto done;
	}
	newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);

	/*
	 * 3. Mark this key as "changed". Any "watchers" will be
	 *    notified as soon as the lock is released.
	 */
	CFSetAddValue(changedKeys, key);

	/*
	 * 4. Add this key to a deferred cleanup list so that, after
	 *    the change notifications are posted, any associated
	 *    regex keys can be removed.
	 */
	CFSetAddValue(deferredRemovals, key);

	/*
	 * 5. Check if this is a session key and, if so, add it
	 *    to the (session) removal list
	 */
	sessionKey = CFDictionaryGetValue(newDict, kSCDSession);
	if (sessionKey) {
		CFStringRef	removedKey;

		/* We are no longer a session key! */
		CFDictionaryRemoveValue(newDict, kSCDSession);

		/* add this session key to the (session) removal list */
		removedKey = CFStringCreateWithFormat(NULL, 0, CFSTR("%@:%@"), sessionKey, key);
		CFSetAddValue(removedSessionKeys, removedKey);
		CFRelease(removedKey);
	}

	/*
	 * 6. Remove data and update/remove the dictionary store entry.
	 */
	CFDictionaryRemoveValue(newDict, kSCDData);
	if (CFDictionaryGetCount(newDict) > 0) {
		/* this key is still being "watched" */
		CFDictionarySetValue(storeData, key, newDict);
	} else {
		/* no information left, remove the empty dictionary */
		CFDictionaryRemoveValue(storeData, key);
	}
	CFRelease(newDict);

	/*
	 * 7. Release our lock.
	 */
    done:
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}


__private_extern__
kern_return_t
_configremove(mach_port_t		server,
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

	*sc_status = __SCDynamicStoreRemoveValue(mySession->store, key, FALSE);

    done :

	if (key)	CFRelease(key);

	return KERN_SUCCESS;
}

