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

#include <unistd.h>

#include "configd.h"
#include "session.h"

static boolean_t
isMySessionKey(CFStringRef sessionKey, CFStringRef key)
{
	CFDictionaryRef	dict;
	CFStringRef	cacheSessionKey;

	dict = CFDictionaryGetValue(cacheData, key);
	if (!dict) {
		/* if key no longer exists */
		return FALSE;
	}

	cacheSessionKey = CFDictionaryGetValue(dict, kSCDSession);
	if (!cacheSessionKey) {
		/* if this is not a session key */
		return FALSE;
	}

	if (!CFEqual(sessionKey, cacheSessionKey)) {
		/* if this is not "my" session key */
		return FALSE;
	}

	return TRUE;
}


SCDStatus
_SCDClose(SCDSessionRef *session)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)*session;
	CFIndex			keyCnt;
	CFStringRef		sessionKey;
	CFDictionaryRef		dict;
	CFArrayRef		keys;
	serverSessionRef	mySession;

	SCDLog(LOG_DEBUG, CFSTR("_SCDClose:"));

	if ((*session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	/* Remove notification keys */
	if ((keyCnt = CFSetGetCount(sessionPrivate->keys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex		i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(sessionPrivate->keys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			(void) _SCDNotifierRemove(*session,
						  CFArrayGetValueAtIndex(keysToRemove, i),
						  0);
		}
		CFRelease(keysToRemove);
	}

	/* Remove regex notification keys */
	if ((keyCnt = CFSetGetCount(sessionPrivate->reKeys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex		i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(sessionPrivate->reKeys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			(void) _SCDNotifierRemove(*session,
						  CFArrayGetValueAtIndex(keysToRemove, i),
						  kSCDRegexKey);
		}
		CFRelease(keysToRemove);
	}

	/* Remove/cancel any outstanding notification requests. */
	(void) _SCDNotifierCancel(*session);

	/* Remove any session keys */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);
	dict = CFDictionaryGetValue(sessionData, sessionKey);
	keys = CFDictionaryGetValue(dict, kSCDSessionKeys);
	if (keys && ((keyCnt = CFArrayGetCount(keys)) > 0)) {
		boolean_t	wasLocked;
		CFIndex		i;

		/*
		 * if necessary, claim a lock to ensure that we inform
		 * any processes that a session key was removed.
		 */
		wasLocked = SCDOptionGet(NULL, kSCDOptionIsLocked);
		if (!wasLocked) {
			(void) _SCDLock(*session);
		}

		/* remove keys from "locked" cache" */
		for (i=0; i<keyCnt; i++) {
			if (isMySessionKey(sessionKey, CFArrayGetValueAtIndex(keys, i)))
				(void) _SCDRemove(*session, CFArrayGetValueAtIndex(keys, i));
		}

		if (wasLocked) {
			/* remove keys from "unlocked" cache" */
			_swapLockedCacheData();
			for (i=0; i<keyCnt; i++) {
				if (isMySessionKey(sessionKey, CFArrayGetValueAtIndex(keys, i)))
					(void) _SCDRemove(*session, CFArrayGetValueAtIndex(keys, i));
			}
			_swapLockedCacheData();
		}

		/*
		 * Note: everyone who calls _SCDClose() ends up
		 *       removing this sessions dictionary. As
		 *       such, we don't need to worry about
		 *       the session keys.
		 */
	}
	CFRelease(sessionKey);

	/* release the lock */
	if (SCDOptionGet(*session, kSCDOptionIsLocked)) {
		(void) _SCDUnlock(*session);
	}

	/*
	 * Invalidate the server port (for this client) which will result
	 * in the removal of any associated run loop sources.
	 */
	mySession = getSession(sessionPrivate->server);
	if (mySession->serverRunLoopSource) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
				      mySession->serverRunLoopSource,
				      kCFRunLoopDefaultMode);
		CFRelease(mySession->serverRunLoopSource);
	}
	CFMachPortInvalidate(mySession->serverPort);
	CFRelease(mySession->serverPort);

	CFRelease(sessionPrivate->keys);
	CFRelease(sessionPrivate->reKeys);
	CFAllocatorDeallocate(NULL, sessionPrivate);
	*session = NULL;

	return SCD_OK;
}


kern_return_t
_configclose(mach_port_t server, int *scd_status)
{
	serverSessionRef	mySession = getSession(server);

	SCDLog(LOG_DEBUG, CFSTR("Close session."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	/*
	 * Close the session.
	 */
	*scd_status = _SCDClose(&mySession->session);
	if (*scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("  _SCDClose(): %s"), SCDError(*scd_status));
		return KERN_SUCCESS;
	}

	/*
	 * Remove the session entry.
	 */
	removeSession(server);

	return KERN_SUCCESS;
}
