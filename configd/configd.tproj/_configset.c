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

#include "configd.h"
#include "session.h"

int
__SCDynamicStoreSetValue(SCDynamicStoreRef store, CFStringRef key, CFPropertyListRef value)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sc_status	= kSCStatusOK;
	CFDictionaryRef			dict;
	CFMutableDictionaryRef		newDict;
	Boolean				newEntry	= FALSE;
	CFStringRef			sessionKey;
	CFStringRef			storeSessionKey;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreSetValue:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  value        = %@"), value);

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
			CFDictionaryApplyFunction(sessionData,
						  (CFDictionaryApplierFunction)_addRegexWatchersBySession,
						  (void *)key);
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
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		xmlData;	/* data (XML serialized) */
	CFPropertyListRef	data;		/* data (un-serialized) */
	CFStringRef		xmlError;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Set key to configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*sc_status = kSCStatusOK;

	/* un-serialize the key */
	xmlKey = CFDataCreate(NULL, keyRef, keyLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)keyRef, keyLen);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	key = CFPropertyListCreateFromXMLData(NULL,
					      xmlKey,
					      kCFPropertyListImmutable,
					      &xmlError);
	CFRelease(xmlKey);
	if (!key) {
		if (xmlError) {
			SCLog(_configd_verbose, LOG_DEBUG,
			       CFSTR("CFPropertyListCreateFromXMLData() key: %@"),
			       xmlError);
			CFRelease(xmlError);
		}
		*sc_status = kSCStatusFailed;
	} else if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	/* un-serialize the data */
	xmlData = CFDataCreate(NULL, dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	data = CFPropertyListCreateFromXMLData(NULL,
					       xmlData,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlData);
	if (!data) {
		if (xmlError) {
			SCLog(_configd_verbose, LOG_DEBUG,
			       CFSTR("CFPropertyListCreateFromXMLData() data: %@"),
			       xmlError);
			CFRelease(xmlError);
		}
		*sc_status = kSCStatusFailed;
	} else if (!isA_CFPropertyList(data)) {
		*sc_status = kSCStatusInvalidArgument;
	}

	if (*sc_status != kSCStatusOK) {
		if (key)	CFRelease(key);
		if (data)	CFRelease(data);
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreSetValue(mySession->store, key, data);
	*newInstance = 0;

	CFRelease(key);
	CFRelease(data);

	return KERN_SUCCESS;
}

static void
setSpecificKey(const void *key, const void *value, void *context)
{
	CFStringRef		k	= (CFStringRef)key;
	CFPropertyListRef	v	= (CFPropertyListRef)value;
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)context;

	if (!isA_CFString(k)) {
		return;
	}

	if (!isA_CFPropertyList(v)) {
		return;
	}

	(void) __SCDynamicStoreSetValue(store, k, v);

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

	(void) __SCDynamicStoreRemoveValue(store, k);

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

	(void) __SCDynamicStoreNotifyValue(store, k);

	return;
}

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
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDictionaryRef		dict	= NULL;		/* key/value dictionary (un-serialized) */
	CFArrayRef		remove	= NULL;		/* keys to remove (un-serialized) */
	CFArrayRef		notify	= NULL;		/* keys to notify (un-serialized) */

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Set key to configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*sc_status = kSCStatusOK;

	if (dictRef && (dictLen > 0)) {
		CFDataRef	xmlDict;	/* key/value dictionary (XML serialized) */
		CFStringRef	xmlError;

		/* un-serialize the key/value pairs to set */
		xmlDict = CFDataCreate(NULL, dictRef, dictLen);
		status = vm_deallocate(mach_task_self(), (vm_address_t)dictRef, dictLen);
		if (status != KERN_SUCCESS) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		dict = CFPropertyListCreateFromXMLData(NULL,
						       xmlDict,
						       kCFPropertyListImmutable,
						       &xmlError);
		CFRelease(xmlDict);
		if (!dict) {
			if (xmlError) {
				SCLog(_configd_verbose, LOG_DEBUG,
				       CFSTR("CFPropertyListCreateFromXMLData() dict: %@"),
				       xmlError);
				CFRelease(xmlError);
			}
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFDictionary(dict)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (removeRef && (removeLen > 0)) {
		CFDataRef	xmlRemove;	/* keys to remove (XML serialized) */
		CFStringRef	xmlError;

		/* un-serialize the keys to remove */
		xmlRemove = CFDataCreate(NULL, removeRef, removeLen);
		status = vm_deallocate(mach_task_self(), (vm_address_t)removeRef, removeLen);
		if (status != KERN_SUCCESS) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		remove = CFPropertyListCreateFromXMLData(NULL,
							 xmlRemove,
							 kCFPropertyListImmutable,
							 &xmlError);
		CFRelease(xmlRemove);
		if (!remove) {
			if (xmlError) {
				SCLog(_configd_verbose, LOG_DEBUG,
				       CFSTR("CFPropertyListCreateFromXMLData() remove: %@"),
				       xmlError);
				CFRelease(xmlError);
			}
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(remove)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (notifyRef && (notifyLen > 0)) {
		CFDataRef	xmlNotify;	/* keys to notify (XML serialized) */
		CFStringRef	xmlError;

		/* un-serialize the keys to notify */
		xmlNotify = CFDataCreate(NULL, notifyRef, notifyLen);
		status = vm_deallocate(mach_task_self(), (vm_address_t)notifyRef, notifyLen);
		if (status != KERN_SUCCESS) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		notify = CFPropertyListCreateFromXMLData(NULL,
						       xmlNotify,
						       kCFPropertyListImmutable,
						       &xmlError);
		CFRelease(xmlNotify);
		if (!notify) {
			if (xmlError) {
				SCLog(_configd_verbose, LOG_DEBUG,
				       CFSTR("CFPropertyListCreateFromXMLData() notify: %@"),
				       xmlError);
				CFRelease(xmlError);
			}
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(notify)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (*sc_status != kSCStatusOK) {
		goto done;
	}

	/*
	 * Ensure that we hold the lock
	 */
	*sc_status = __SCDynamicStoreLock(mySession->store, TRUE);
	if (*sc_status != kSCStatusOK) {
		goto done;
	}

	/*
	 * Set the new/updated keys
	 */
	if (dict) {
		CFDictionaryApplyFunction(dict,
					  setSpecificKey,
					  (void *)mySession->store);
	}

	/*
	 * Remove the specified keys
	 */
	if (remove) {
		CFArrayApplyFunction(remove,
				     CFRangeMake(0, CFArrayGetCount(remove)),
				     removeSpecificKey,
				     (void *)mySession->store);
	}

	/*
	 * Notify the specified keys
	 */
	if (notify) {
		CFArrayApplyFunction(notify,
				     CFRangeMake(0, CFArrayGetCount(notify)),
				     notifySpecificKey,
				     (void *)mySession->store);
	}

	__SCDynamicStoreUnlock(mySession->store, TRUE);	/* Release our lock */

    done :

	if (dict)	CFRelease(dict);
	if (remove)	CFRelease(remove);
	if (notify)	CFRelease(notify);

	return KERN_SUCCESS;
}
