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


#include "configd.h"
#include "configd_server.h"
#include "session.h"


static void
_notifyWatchers()
{
	CFIndex			keyCnt;
	void			**keys;

	if ((keyCnt = CFSetGetCount(changedKeys)) == 0)
		return;		/* if nothing to do */

	keys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
	CFSetGetValues(changedKeys, keys);
	while (--keyCnt >= 0) {
		CFDictionaryRef		dict;
		CFArrayRef		sessionsWatchingKey;
		CFIndex			watcherCnt;
		void			**watchers;
		CFDictionaryRef		info;
		CFMutableDictionaryRef	newInfo;
		CFArrayRef		changes;
		CFMutableArrayRef	newChanges;

		dict = CFDictionaryGetValue(cacheData, (CFStringRef)keys[keyCnt]);
		if ((dict == NULL) || (CFDictionaryContainsKey(dict, kSCDWatchers) == FALSE)) {
			/* key doesn't exist or nobody cares if it changed */
			continue;
		}

		/*
		 * Add this key to the list of changes for each of the
		 * sessions which is "watching".
		 */
		sessionsWatchingKey = CFDictionaryGetValue(dict, kSCDWatchers);
		watcherCnt = CFArrayGetCount(sessionsWatchingKey);
		watchers   = CFAllocatorAllocate(NULL, watcherCnt * sizeof(CFNumberRef), 0);
		CFArrayGetValues(sessionsWatchingKey,
				 CFRangeMake(0, CFArrayGetCount(sessionsWatchingKey)),
				 watchers);
		while (--watcherCnt >= 0) {
			CFStringRef	sessionKey;

			sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), watchers[watcherCnt]);
			info = CFDictionaryGetValue(sessionData, sessionKey);
			if (info) {
				newInfo = CFDictionaryCreateMutableCopy(NULL, 0, info);
			} else {
				newInfo = CFDictionaryCreateMutable(NULL,
								    0,
								    &kCFTypeDictionaryKeyCallBacks,
								    &kCFTypeDictionaryValueCallBacks);
			}

			changes = CFDictionaryGetValue(newInfo, kSCDChangedKeys);
			if (changes) {
				newChanges = CFArrayCreateMutableCopy(NULL, 0, changes);
			} else {
				newChanges = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}

			if (CFArrayContainsValue(newChanges,
						 CFRangeMake(0, CFArrayGetCount(newChanges)),
						 (CFStringRef)keys[keyCnt]) == FALSE) {
				CFArrayAppendValue(newChanges, (CFStringRef)keys[keyCnt]);
			}
			CFDictionarySetValue(newInfo, kSCDChangedKeys, newChanges);
			CFRelease(newChanges);
			CFDictionarySetValue(sessionData, sessionKey, newInfo);
			CFRelease(newInfo);
			CFRelease(sessionKey);

			/*
			 * flag this session as needing a kick
			 */
			if (needsNotification == NULL)
				needsNotification = CFSetCreateMutable(NULL,
								       0,
								       &kCFTypeSetCallBacks);
			CFSetAddValue(needsNotification, watchers[watcherCnt]);
		}
		CFAllocatorDeallocate(NULL, watchers);
	}
	CFAllocatorDeallocate(NULL, keys);

	/*
	 * The list of changed keys have been updated for any sessions
	 * monitoring changes to the "cache". The next step, handled by
	 * the "configd" server, is to push out any needed notifications.
	 */
	CFSetRemoveAllValues(changedKeys);

}


static void
_processDeferredRemovals()
{
	CFIndex			keyCnt;
	void			**keys;

	if ((keyCnt = CFSetGetCount(deferredRemovals)) == 0)
		return;		/* if nothing to do */

	keys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
	CFSetGetValues(deferredRemovals, keys);
	while (--keyCnt >= 0) {
		CFDictionaryApplyFunction(sessionData,
					  (CFDictionaryApplierFunction)_removeRegexWatchersBySession,
					  keys[keyCnt]);
	}
	CFAllocatorDeallocate(NULL, keys);

	/*
	 * All regex keys associated with removed cache dictionary keys have
	 * been removed. Start the list fresh again.
	 */
	CFSetRemoveAllValues(deferredRemovals);

	return;
}


static void
_cleanupRemovedSessionKeys(const void *value, void *context)
{
	CFStringRef		removedKey = (CFStringRef)value;
	CFRange			dRange;
	CFStringRef		sessionKey;
	CFStringRef		key;
	CFDictionaryRef		sessionDict;
	CFArrayRef		sessionKeys;
	CFIndex			i;
	CFMutableDictionaryRef	newSessionDict;

	dRange     = CFStringFind(removedKey, CFSTR(":"), 0);
	sessionKey = CFStringCreateWithSubstring(NULL,
						 removedKey,
						 CFRangeMake(0, dRange.location));
	key        = CFStringCreateWithSubstring(NULL,
						 removedKey,
						 CFRangeMake(dRange.location+dRange.length,
							     CFStringGetLength(removedKey)-dRange.location-dRange.length));

	/*
	 * remove the key from the session key list
	 */
	sessionDict = CFDictionaryGetValue(sessionData, sessionKey);
	if (!sessionDict) {
		/* if no session */
		goto done;
	}

	sessionKeys = CFDictionaryGetValue(sessionDict, kSCDSessionKeys);
	if (!sessionKeys) {
		/* if no session keys */
		goto done;
	}

	i = CFArrayGetFirstIndexOfValue(sessionKeys,
					CFRangeMake(0, CFArrayGetCount(sessionKeys)),
					key);
	if (i == -1) {
		/* if this session key has already been removed */
		goto done;
	}

	newSessionDict = CFDictionaryCreateMutableCopy(NULL, 0, sessionDict);
	if (CFArrayGetCount(sessionKeys) == 1) {
		/* remove the last (session) key */
		CFDictionaryRemoveValue(newSessionDict, kSCDSessionKeys);
	} else {
		CFMutableArrayRef	newSessionKeys;

		/* remove the (session) key */
		newSessionKeys = CFArrayCreateMutableCopy(NULL, 0, sessionKeys);
		CFArrayRemoveValueAtIndex(newSessionKeys, i);
		CFDictionarySetValue(newSessionDict, kSCDSessionKeys, newSessionKeys);
		CFRelease(newSessionKeys);
	}
	CFDictionarySetValue(sessionData, sessionKey, newSessionDict);
	CFRelease(newSessionDict);

    done:

	CFRelease(sessionKey);
	CFRelease(key);

	return;
}


SCDStatus
_SCDUnlock(SCDSessionRef session)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	serverSessionRef	mySession;

	SCDLog(LOG_DEBUG, CFSTR("_SCDUnlock:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	if (!SCDOptionGet(NULL, kSCDOptionIsLocked) || !SCDOptionGet(session, kSCDOptionIsLocked)) {
		return SCD_NEEDLOCK;	/* sorry, you don't have the lock */
	}

	/*
	 * all of the changes can be committed to the (real) cache.
	 */
	CFDictionaryRemoveAllValues(cacheData_s);
	CFSetRemoveAllValues       (changedKeys_s);
	CFSetRemoveAllValues       (deferredRemovals_s);
	CFSetRemoveAllValues       (removedSessionKeys_s);

#ifdef	DEBUG
	SCDLog(LOG_DEBUG, CFSTR("keys I changed           = %@"), changedKeys);
	SCDLog(LOG_DEBUG, CFSTR("keys flagged for removal = %@"), deferredRemovals);
	SCDLog(LOG_DEBUG, CFSTR("keys I'm watching        = %@"), sessionPrivate->keys);
	SCDLog(LOG_DEBUG, CFSTR("regex keys I'm watching  = %@"), sessionPrivate->reKeys);
#endif	/* DEBUG */

	/*
	 * push notifications to any session watching those keys which
	 * were recently changed.
	 */
	_notifyWatchers();

	/*
	 * process any deferred key deletions.
	 */
	_processDeferredRemovals();

	/*
	 * clean up any removed session keys
	 */
	CFSetApplyFunction(removedSessionKeys, _cleanupRemovedSessionKeys, NULL);
	CFSetRemoveAllValues(removedSessionKeys);

#ifdef	DEBUG
	SCDLog(LOG_DEBUG, CFSTR("sessions to notify = %@"), needsNotification);
#endif	/* DEBUG */

	/* Remove the "locked" run loop source for this port */
	mySession = getSession(sessionPrivate->server);
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mySession->serverRunLoopSource, CFSTR("locked"));

	SCDOptionSet(NULL,    kSCDOptionIsLocked, FALSE);	/* global lock flag */
	SCDOptionSet(session, kSCDOptionIsLocked, FALSE);	/* per-session lock flag */

	return SCD_OK;
}


kern_return_t
_configunlock(mach_port_t server, int *scd_status)
{
	serverSessionRef	mySession = getSession(server);

	SCDLog(LOG_DEBUG, CFSTR("Unlock configuration database."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	*scd_status = _SCDUnlock(mySession->session);
	if (*scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("  _SCDUnlock(): %s"), SCDError(*scd_status));
		return KERN_SUCCESS;
	}

	return KERN_SUCCESS;
}
