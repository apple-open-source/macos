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
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <unistd.h>

#include "configd.h"
#include "session.h"

static Boolean
isMySessionKey(CFStringRef sessionKey, CFStringRef key)
{
	CFDictionaryRef	dict;
	CFStringRef	storeSessionKey;

	dict = CFDictionaryGetValue(storeData, key);
	if (!dict) {
		/* if key no longer exists */
		return FALSE;
	}

	storeSessionKey = CFDictionaryGetValue(dict, kSCDSession);
	if (!storeSessionKey) {
		/* if this is not a session key */
		return FALSE;
	}

	if (!CFEqual(sessionKey, storeSessionKey)) {
		/* if this is not "my" session key */
		return FALSE;
	}

	return TRUE;
}


int
__SCDynamicStoreClose(SCDynamicStoreRef *store)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)*store;
	CFIndex				keyCnt;
	CFStringRef			sessionKey;
	CFDictionaryRef			dict;
	CFArrayRef			keys;
	serverSessionRef		mySession;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreClose:"));

	if ((*store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	/* Remove notification keys */
	if ((keyCnt = CFSetGetCount(storePrivate->keys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex		i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(storePrivate->keys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			(void) __SCDynamicStoreRemoveWatchedKey(*store,
								CFArrayGetValueAtIndex(keysToRemove, i),
								FALSE);
		}
		CFRelease(keysToRemove);
	}

	/* Remove regex notification keys */
	if ((keyCnt = CFSetGetCount(storePrivate->reKeys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex		i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(storePrivate->reKeys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			(void) __SCDynamicStoreRemoveWatchedKey(*store,
								CFArrayGetValueAtIndex(keysToRemove, i),
								TRUE);
		}
		CFRelease(keysToRemove);
	}

	/* Remove/cancel any outstanding notification requests. */
	(void) __SCDynamicStoreNotifyCancel(*store);

	/* Remove any session keys */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);
	dict = CFDictionaryGetValue(sessionData, sessionKey);
	keys = CFDictionaryGetValue(dict, kSCDSessionKeys);
	if (keys && ((keyCnt = CFArrayGetCount(keys)) > 0)) {
		Boolean	wasLocked;
		CFIndex	i;

		/*
		 * if necessary, claim a lock to ensure that we inform
		 * any processes that a session key was removed.
		 */
		wasLocked = (storeLocked > 0);
		if (!wasLocked) {
			(void) __SCDynamicStoreLock(*store, FALSE);
		}

		/* remove keys from "locked" store" */
		for (i=0; i<keyCnt; i++) {
			if (isMySessionKey(sessionKey, CFArrayGetValueAtIndex(keys, i))) {
				(void) __SCDynamicStoreRemoveValue(*store, CFArrayGetValueAtIndex(keys, i));
			}
		}

		if (wasLocked) {
			/* remove keys from "unlocked" store" */
			_swapLockedStoreData();
			for (i=0; i<keyCnt; i++) {
				if (isMySessionKey(sessionKey, CFArrayGetValueAtIndex(keys, i)))
					(void) __SCDynamicStoreRemoveValue(*store, CFArrayGetValueAtIndex(keys, i));
			}
			_swapLockedStoreData();
		}

		/*
		 * Note: everyone who calls __SCDynamicStoreClose() ends
		 *       up removing this sessions dictionary. As such,
		 *       we don't need to worry about the session keys.
		 */
	}
	CFRelease(sessionKey);

	/* release the lock */
	if (storePrivate->locked) {
		(void) __SCDynamicStoreUnlock(*store, FALSE);
	}

	/*
	 * Remove the run loop source on the server port (for this
	 * client).  Then, invalidate and release the port.
	 */
	mySession = getSession(storePrivate->server);
	if (mySession->serverRunLoopSource) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
				      mySession->serverRunLoopSource,
				      kCFRunLoopDefaultMode);
		CFRelease(mySession->serverRunLoopSource);
	}
	CFMachPortInvalidate(mySession->serverPort);
	CFRelease(mySession->serverPort);

	storePrivate->server = MACH_PORT_NULL;
	CFRelease(*store);
	*store = NULL;

	return kSCStatusOK;
}


kern_return_t
_configclose(mach_port_t server, int *sc_status)
{
	serverSessionRef	mySession = getSession(server);

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Close session."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	/*
	 * Close the session.
	 */
	*sc_status = __SCDynamicStoreClose(&mySession->store);
	if (*sc_status != kSCStatusOK) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreClose(): %s"), SCErrorString(*sc_status));
		return KERN_SUCCESS;
	}

	/*
	 * Remove the session entry.
	 */
	removeSession(server);

	return KERN_SUCCESS;
}
