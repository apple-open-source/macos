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
#include "pattern.h"


__private_extern__
int
__SCDynamicStoreSetValue(SCDynamicStoreRef store, CFStringRef key, CFDataRef value, Boolean internal)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFDictionaryRef			dict;
	CFMutableDictionaryRef		newDict;
	Boolean				newEntry	= FALSE;
	CFStringRef			sessionKey;
	CFStringRef			storeSessionKey;

	if (_configd_verbose) {
		CFPropertyListRef	val;

		(void) _SCUnserialize(&val, value, NULL, NULL);
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreSetValue:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  key          = %@"), key);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  value        = %@"), val);
		CFRelease(val);
	}

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("%s : %5d : %@\n"),
			internal ? "*set   " : "set    ",
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
	 * 2. Grab the current (or establish a new) dictionary for this key.
	 */

	dict = CFDictionaryGetValue(storeData, key);
	if (dict) {
		newDict = CFDictionaryCreateMutableCopy(NULL,
							0,
							dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	/*
	 * 3. Update the dictionary entry to be saved to the store.
	 */
	newEntry = !CFDictionaryContainsKey(newDict, kSCDData);
	CFDictionarySetValue(newDict, kSCDData, value);

	/*
	 * 4. Since we are updating this key we need to check and, if
	 *    necessary, remove the indication that this key is on
	 *    another session's remove-on-close list.
	 */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);
	if (CFDictionaryGetValueIfPresent(newDict, kSCDSession, (void *)&storeSessionKey) &&
	    !CFEqual(sessionKey, storeSessionKey)) {
		CFStringRef	removedKey;

		/* We are no longer a session key! */
		CFDictionaryRemoveValue(newDict, kSCDSession);

		/* add this session key to the (session) removal list */
		removedKey = CFStringCreateWithFormat(NULL, 0, CFSTR("%@:%@"), storeSessionKey, key);
		CFSetAddValue(removedSessionKeys, removedKey);
		CFRelease(removedKey);
	}
	CFRelease(sessionKey);

	/*
	 * 5. Update the dictionary entry in the store.
	 */

	CFDictionarySetValue(storeData, key, newDict);
	CFRelease(newDict);

	/*
	 * 6. For "new" entries to the store, check the deferred cleanup
	 *    list. If the key is flagged for removal, remove it from the
	 *    list since any defined regex's for this key are still defined
	 *    and valid. If the key is not flagged then iterate over the
	 *    sessionData dictionary looking for regex keys which match the
	 *    updated key. If a match is found then we mark those keys as
	 *    being watched.
	 */

	if (newEntry) {
		if (CFSetContainsValue(deferredRemovals, key)) {
			CFSetRemoveValue(deferredRemovals, key);
		} else {
			patternAddKey(key);
		}
	}

	/*
	 * 7. Mark this key as "changed". Any "watchers" will be notified
	 *    as soon as the lock is released.
	 */
	CFSetAddValue(changedKeys, key);

	/*
	 * 8. Release our lock.
	 */
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}

__private_extern__
kern_return_t
_configset(mach_port_t			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   xmlData_t			dataRef,	/* raw XML bytes */
	   mach_msg_type_number_t	dataLen,
	   int				oldInstance,
	   int				*newInstance,
	   int				*sc_status
)
{
	serverSessionRef	mySession = getSession(server);
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		data;		/* data (un-serialized) */

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Set key to configuration database."));
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

	*sc_status = __SCDynamicStoreSetValue(mySession->store, key, data, FALSE);
	*newInstance = 0;

	CFRelease(key);
	CFRelease(data);

	return KERN_SUCCESS;
}

static void
setSpecificKey(const void *key, const void *value, void *context)
{
	CFStringRef		k	= (CFStringRef)key;
	CFDataRef		v	= (CFDataRef)value;
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)context;

	if (!isA_CFString(k)) {
		return;
	}

	if (!isA_CFData(v)) {
		return;
	}

	(void) __SCDynamicStoreSetValue(store, k, v, TRUE);

	return;
}

static void
removeSpecificKey(const void *value, void *context)
{
	CFStringRef		k	= (CFStringRef)value;
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)context;

	if (!isA_CFString(k)) {
		return;
	}

	(void) __SCDynamicStoreRemoveValue(store, k, TRUE);

	return;
}

static void
notifySpecificKey(const void *value, void *context)
{
	CFStringRef		k	= (CFStringRef)value;
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)context;

	if (!isA_CFString(k)) {
		return;
	}

	(void) __SCDynamicStoreNotifyValue(store, k, TRUE);

	return;
}

__private_extern__
int
__SCDynamicStoreSetMultiple(SCDynamicStoreRef store, CFDictionaryRef keysToSet, CFArrayRef keysToRemove, CFArrayRef keysToNotify)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;

	if (_configd_verbose) {
		CFDictionaryRef	expDict;

		expDict = keysToSet ? _SCUnserializeMultiple(keysToSet) : NULL;
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreSetMultiple:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keysToSet    = %@"), expDict);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keysToRemove = %@"), keysToRemove);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keysToNotify = %@"), keysToNotify);
		if (expDict) CFRelease(expDict);
	}

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("set m   : %5d : %d set, %d remove, %d notify\n"),
			storePrivate->server,
			keysToSet    ? CFDictionaryGetCount(keysToSet)    : 0,
			keysToRemove ? CFArrayGetCount     (keysToRemove) : 0,
			keysToNotify ? CFArrayGetCount     (keysToNotify) : 0);
	}

	/*
	 * Ensure that we hold the lock
	 */
	sc_status = __SCDynamicStoreLock(store, TRUE);
	if (sc_status != kSCStatusOK) {
		return sc_status;
	}

	/*
	 * Set the new/updated keys
	 */
	if (keysToSet) {
		CFDictionaryApplyFunction(keysToSet,
					  setSpecificKey,
					  (void *)store);
	}

	/*
	 * Remove the specified keys
	 */
	if (keysToRemove) {
		CFArrayApplyFunction(keysToRemove,
				     CFRangeMake(0, CFArrayGetCount(keysToRemove)),
				     removeSpecificKey,
				     (void *)store);
	}

	/*
	 * Notify the specified keys
	 */
	if (keysToNotify) {
		CFArrayApplyFunction(keysToNotify,
				     CFRangeMake(0, CFArrayGetCount(keysToNotify)),
				     notifySpecificKey,
				     (void *)store);
	}

	/* Release our lock */
	__SCDynamicStoreUnlock(store, TRUE);

	return sc_status;
}

__private_extern__
kern_return_t
_configset_m(mach_port_t		server,
	     xmlData_t			dictRef,
	     mach_msg_type_number_t	dictLen,
	     xmlData_t			removeRef,
	     mach_msg_type_number_t	removeLen,
	     xmlData_t			notifyRef,
	     mach_msg_type_number_t	notifyLen,
	     int			*sc_status)
{
	serverSessionRef	mySession = getSession(server);
	CFDictionaryRef		dict	= NULL;		/* key/value dictionary (un-serialized) */
	CFArrayRef		remove	= NULL;		/* keys to remove (un-serialized) */
	CFArrayRef		notify	= NULL;		/* keys to notify (un-serialized) */

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Set/remove/notify keys to configuration database."));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  server = %d"), server);
	}

	*sc_status = kSCStatusOK;

	if (dictRef && (dictLen > 0)) {
		/* un-serialize the key/value pairs to set */
		if (!_SCUnserialize((CFPropertyListRef *)&dict, NULL, (void *)dictRef, dictLen)) {
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFDictionary(dict)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (removeRef && (removeLen > 0)) {
		/* un-serialize the keys to remove */
		if (!_SCUnserialize((CFPropertyListRef *)&remove, NULL, (void *)removeRef, removeLen)) {
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(remove)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (notifyRef && (notifyLen > 0)) {
		/* un-serialize the keys to notify */
		if (!_SCUnserialize((CFPropertyListRef *)&notify, NULL, (void *)notifyRef, notifyLen)) {
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(notify)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (!mySession) {
		/* you must have an open session to play */
		*sc_status = kSCStatusNoStoreSession;
	}

	if (*sc_status != kSCStatusOK) {
		goto done;
	}

	*sc_status = __SCDynamicStoreSetMultiple(mySession->store, dict, remove, notify);

    done :

	if (dict)	CFRelease(dict);
	if (remove)	CFRelease(remove);
	if (notify)	CFRelease(notify);

	return KERN_SUCCESS;
}
