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
#include "session.h"

SCDStatus
_SCDSet(SCDSessionRef session, CFStringRef key, SCDHandleRef handle)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	SCDStatus		scd_status = SCD_OK;
	boolean_t		wasLocked;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFNumberRef		num;
	int			dictInstance;
	CFStringRef		sessionKey;
	CFStringRef		cacheSessionKey;

	SCDLog(LOG_DEBUG, CFSTR("_SCDSet:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  data         = %@"), SCDHandleGetData(handle));
	SCDLog(LOG_DEBUG, CFSTR("  instance     = %d"), SCDHandleGetInstance(handle));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/*
	 * 1. Determine if the cache lock is currently held
	 *    and acquire the lock if necessary.
	 */
	wasLocked = SCDOptionGet(NULL, kSCDOptionIsLocked);
	if (!wasLocked) {
		scd_status = _SCDLock(session);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_DEBUG, CFSTR("  _SCDLock(): %s"), SCDError(scd_status));
			return scd_status;
		}
	}

	/*
	 * 2. Grab the current (or establish a new) dictionary for this key.
	 */

	dict = CFDictionaryGetValue(cacheData, key);
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
	 * 3. Make sure that we're not updating the cache with potentially
	 *    stale information.
	 */

	if ((num = CFDictionaryGetValue(newDict, kSCDInstance)) == NULL) {
		/* if first instance */
		dictInstance = 0;
		_SCDHandleSetInstance(handle, dictInstance);
	} else {
		(void) CFNumberGetValue(num, kCFNumberIntType, &dictInstance);
	}
	if (SCDHandleGetInstance(handle) != dictInstance) {
		/* data may be based on old information */
		CFRelease(newDict);
		scd_status = SCD_STALE;
		goto done;
	}

	/*
	 * 4. Update the dictionary entry (data & instance) to be saved to
	 *    the cache.
	 */

	CFDictionarySetValue(newDict, kSCDData, SCDHandleGetData(handle));

	dictInstance++;
	num = CFNumberCreate(NULL, kCFNumberIntType, &dictInstance);
	CFDictionarySetValue(newDict, kSCDInstance, num);
	CFRelease(num);
	_SCDHandleSetInstance(handle, dictInstance);
	SCDLog(LOG_DEBUG, CFSTR("  new instance = %d"), SCDHandleGetInstance(handle));

	/*
	 * 5. Since we are updating this key we need to check and, if
	 *    necessary, remove the indication that this key is on
	 *    another session's remove-on-close list.
	 */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);
	if (CFDictionaryGetValueIfPresent(newDict, kSCDSession, (void *)&cacheSessionKey) &&
	    !CFEqual(sessionKey, cacheSessionKey)) {
		CFStringRef	removedKey;

		/* We are no longer a session key! */
		CFDictionaryRemoveValue(newDict, kSCDSession);

		/* add this session key to the (session) removal list */
		removedKey = CFStringCreateWithFormat(NULL, 0, CFSTR("%@:%@"), cacheSessionKey, key);
		CFSetAddValue(removedSessionKeys, removedKey);
		CFRelease(removedKey);
	}
	CFRelease(sessionKey);

	/*
	 * 6. Update the dictionary entry in the cache.
	 */

	CFDictionarySetValue(cacheData, key, newDict);
	CFRelease(newDict);

	/*
	 * 7. For "new" entries to the cache, check the deferred cleanup
	 *    list. If the key is flagged for removal, remove it from the
	 *    list since any defined regex's for this key are still defined
	 *    and valid. If the key is not flagged then iterate over the
	 *    sessionData dictionary looking for regex keys which match the
	 *    updated key. If a match is found then we mark those keys as
	 *    being watched.
	 */

	if (dictInstance == 1) {
		if (CFSetContainsValue(deferredRemovals, key)) {
			CFSetRemoveValue(deferredRemovals, key);
		} else {
			CFDictionaryApplyFunction(sessionData,
						  (CFDictionaryApplierFunction)_addRegexWatchersBySession,
						  (void *)key);
		}
	}

	/*
	 * 8. Mark this key as "changed". Any "watchers" will be notified
	 *    as soon as the lock is released.
	 */
	CFSetAddValue(changedKeys, key);

	/*
	 * 9. Release the lock if we acquired it as part of this request.
	 */
    done:
	if (!wasLocked)
		_SCDUnlock(session);

	return scd_status;
}


kern_return_t
_configset(mach_port_t			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   xmlData_t			dataRef,	/* raw XML bytes */
	   mach_msg_type_number_t	dataLen,
	   int				oldInstance,
	   int				*newInstance,
	   int				*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		xmlData;	/* data (XML serialized) */
	CFPropertyListRef	data;		/* data (un-serialized) */
	SCDHandleRef		handle;
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("Set key to configuration database."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	/* un-serialize the key */
	xmlKey = CFDataCreate(NULL, keyRef, keyLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)keyRef, keyLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	key = CFPropertyListCreateFromXMLData(NULL,
					      xmlKey,
					      kCFPropertyListImmutable,
					      &xmlError);
	CFRelease(xmlKey);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() key: %s"), xmlError);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	/* un-serialize the data */
	xmlData = CFDataCreate(NULL, dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	data = CFPropertyListCreateFromXMLData(NULL,
					       xmlData,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlData);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() data: %s"), xmlError);
		CFRelease(key);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	handle = SCDHandleInit();
	SCDHandleSetData(handle, data);
	_SCDHandleSetInstance(handle, oldInstance);
	*scd_status = _SCDSet(mySession->session, key, handle);
	if (*scd_status == SCD_OK) {
		*newInstance = SCDHandleGetInstance(handle);
	}
	SCDHandleRelease(handle);
	CFRelease(key);
	CFRelease(data);

	return KERN_SUCCESS;
}
