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
_SCDRemove(SCDSessionRef session, CFStringRef key)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	SCDStatus		scd_status = SCD_OK;
	boolean_t		wasLocked;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFStringRef		sessionKey;

	SCDLog(LOG_DEBUG, CFSTR("_SCDRemove:"));
	SCDLog(LOG_DEBUG, CFSTR("  key      = %@"), key);

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/*
	 * 1. Determine if the cache lock is currently held  and acquire
	 *    the lock if necessary.
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
	 * 2. Ensure that this key exists.
	 */
	dict = CFDictionaryGetValue(cacheData, key);
	if ((dict == NULL) || (CFDictionaryContainsKey(dict, kSCDData) == FALSE)) {
		/* key doesn't exist (or data never defined) */
		scd_status = SCD_NOKEY;
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
	 *    the dictionary cache entry.
	 */
	CFDictionaryRemoveValue(newDict, kSCDData);
	CFDictionaryRemoveValue(newDict, kSCDInstance);
	if (CFDictionaryGetCount(newDict) > 0) {
		/* this key is still being "watched" */
		CFDictionarySetValue(cacheData, key, newDict);
	} else {
		/* no information left, remove the empty dictionary */
		CFDictionaryRemoveValue(cacheData, key);
	}
	CFRelease(newDict);

	/*
	 * 7. Release the lock if we acquired it as part of this request.
	 */
    done:
	if (!wasLocked)
		_SCDUnlock(session);

	return scd_status;
}


kern_return_t
_configremove(mach_port_t		server,
	      xmlData_t			keyRef,		/* raw XML bytes */
	      mach_msg_type_number_t	keyLen,
	      int			*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("Remove key from configuration database."));
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

	*scd_status = _SCDRemove(mySession->session, key);
	CFRelease(key);

	return KERN_SUCCESS;
}

