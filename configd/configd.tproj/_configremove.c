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
__SCDynamicStoreRemoveValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status = kSCStatusOK;
	CFDictionaryRef			dict;
	CFMutableDictionaryRef		newDict;
	CFStringRef			sessionKey;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreRemoveValue:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key      = %@"), key);

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
	 * 6. Remove data, remove instance, and update/remove
	 *    the dictionary store entry.
	 */
	CFDictionaryRemoveValue(newDict, kSCDData);
	CFDictionaryRemoveValue(newDict, kSCDInstance);
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


kern_return_t
_configremove(mach_port_t		server,
	      xmlData_t			keyRef,		/* raw XML bytes */
	      mach_msg_type_number_t	keyLen,
	      int			*sc_status
)
{
	serverSessionRef	mySession = getSession(server);
	CFStringRef		key;		/* key  (un-serialized) */

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Remove key from configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	/* un-serialize the key */
	if (!_SCUnserialize((CFPropertyListRef *)&key, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	if (!isA_CFString(key)) {
		CFRelease(key);
		*sc_status = kSCStatusInvalidArgument;
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreRemoveValue(mySession->store, key);
	CFRelease(key);

	return KERN_SUCCESS;
}

