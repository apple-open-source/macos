/*
 * Copyright (c) 2000, 2001, 2003, 2004, 2006-2009 Apple Inc. All rights reserved.
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

#include <unistd.h>

#include "configd.h"
#include "session.h"

#define	N_QUICK	16

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


static void
removeAllKeys(SCDynamicStoreRef store, Boolean isRegex)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	CFSetRef			keys;
	CFIndex				n;

	keys = isRegex ? storePrivate->patterns : storePrivate->keys;
	n = CFSetGetCount(keys);
	if (n > 0) {
		CFIndex		i;
		CFArrayRef	keysToRemove;
		const void *	watchedKeys_q[N_QUICK];
		const void **	watchedKeys	= watchedKeys_q;

		if (n > (CFIndex)(sizeof(watchedKeys_q) / sizeof(CFStringRef)))
			watchedKeys = CFAllocatorAllocate(NULL, n * sizeof(CFStringRef), 0);
		CFSetGetValues(keys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, n, &kCFTypeArrayCallBacks);
		if (watchedKeys != watchedKeys_q) CFAllocatorDeallocate(NULL, watchedKeys);
		for (i = 0; i < n; i++) {
			(void) __SCDynamicStoreRemoveWatchedKey(store,
								CFArrayGetValueAtIndex(keysToRemove, i),
								isRegex,
								TRUE);
		}
		CFRelease(keysToRemove);
	}

	return;
}


__private_extern__
int
__SCDynamicStoreClose(SCDynamicStoreRef *store, Boolean internal)
{
	CFDictionaryRef			dict;
	CFArrayRef			keys;
	CFIndex				keyCnt;
	serverSessionRef		mySession;
	CFStringRef			sessionKey;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)*store;

	if ((*store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("%s : %5d\n"),
			internal ? "*close " : "close  ",
			storePrivate->server);
	}

	/* Remove all notification keys and patterns */
	removeAllKeys(*store, FALSE);	// keys
	removeAllKeys(*store, TRUE);	// patterns

	/* Remove/cancel any outstanding notification requests. */
	__MACH_PORT_DEBUG(storePrivate->notifyPort != MACH_PORT_NULL, "*** __SCDynamicStoreClose", storePrivate->notifyPort);
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
		for (i = 0; i < keyCnt; i++) {
			if (isMySessionKey(sessionKey, CFArrayGetValueAtIndex(keys, i))) {
				(void) __SCDynamicStoreRemoveValue(*store, CFArrayGetValueAtIndex(keys, i), TRUE);
			}
		}

		if (wasLocked) {
			/* remove keys from "unlocked" store" */
			_swapLockedStoreData();
			for (i = 0; i < keyCnt; i++) {
				if (isMySessionKey(sessionKey, CFArrayGetValueAtIndex(keys, i)))
					(void) __SCDynamicStoreRemoveValue(*store, CFArrayGetValueAtIndex(keys, i), TRUE);
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
	 * invalidate and release our run loop source on the server
	 * port (for this client).  Then, release the port.
	 */
	mySession = getSession(storePrivate->server);
	if (mySession->serverRunLoopSource) {
		CFRunLoopSourceInvalidate(mySession->serverRunLoopSource);
		CFRelease(mySession->serverRunLoopSource);
		mySession->serverRunLoopSource = NULL;
	}
	if (mySession->serverPort != NULL) {
		CFMachPortInvalidate(mySession->serverPort);
		CFRelease(mySession->serverPort);
		mySession->serverPort = NULL;
	}

	storePrivate->server = MACH_PORT_NULL;
	CFRelease(*store);
	*store = NULL;

	return kSCStatusOK;
}


__private_extern__
kern_return_t
_configclose(mach_port_t server, int *sc_status)
{
	serverSessionRef	mySession = getSession(server);

	if (mySession == NULL) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		return KERN_SUCCESS;
	}

	/*
	 * Close the session.
	 */
	__MACH_PORT_DEBUG(TRUE, "*** _configclose", server);
	*sc_status = __SCDynamicStoreClose(&mySession->store, FALSE);
	if (*sc_status != kSCStatusOK) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("_configclose __SCDynamicStoreClose() failed, status = %s"),
		      SCErrorString(*sc_status));
		return KERN_SUCCESS;
	}
	__MACH_PORT_DEBUG(TRUE, "*** _configclose (after __SCDynamicStoreClose)", server);

	/*
	 * Remove our receive right.
	 *
	 * Note: there is no need to cancel the notification request because the
	 *       kernel will have no way to deliver the notification once the
	 *       receive right has been removed.
	 */
	(void) mach_port_mod_refs(mach_task_self(), server, MACH_PORT_RIGHT_RECEIVE, -1);

	/*
	 * Remove the session entry.
	 */
	removeSession(server);

	return KERN_SUCCESS;
}
