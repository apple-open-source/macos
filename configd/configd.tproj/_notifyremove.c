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
_SCDNotifierRemove(SCDSessionRef session, CFStringRef key, int regexOptions)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;

	SCDLog(LOG_DEBUG, CFSTR("_SCDNotifierRemove:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  regexOptions = %0o"), regexOptions);

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/*
	 * remove key from this sessions notifier list after checking that
	 * it was previously defined.
	 */
	if (regexOptions & kSCDRegexKey) {
		if (!CFSetContainsValue(sessionPrivate->reKeys, key))
			return SCD_NOKEY;		/* sorry, key does not exist in notifier list */
		CFSetRemoveValue(sessionPrivate->reKeys, key);	/* remove key from this sessions notifier list */
	} else {
		if (!CFSetContainsValue(sessionPrivate->keys, key))
			return SCD_NOKEY;		/* sorry, key does not exist in notifier list */
		CFSetRemoveValue(sessionPrivate->keys, key);	/* remove key from this sessions notifier list */
	}

	if (regexOptions & kSCDRegexKey) {
		CFStringRef		sessionKey;
		CFDictionaryRef		info;
		CFMutableDictionaryRef	newInfo;
		CFArrayRef		rKeys;
		CFMutableArrayRef	newRKeys;
		CFArrayRef		rData;
		CFMutableArrayRef	newRData;
		CFIndex			i;
		CFDataRef		regexData;
		removeContext		context;

		sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);

		info    = CFDictionaryGetValue(sessionData, sessionKey);
		newInfo = CFDictionaryCreateMutableCopy(NULL, 0, info);

		rKeys    = CFDictionaryGetValue(newInfo, kSCDRegexKeys);
		newRKeys = CFArrayCreateMutableCopy(NULL, 0, rKeys);

		rData    = CFDictionaryGetValue(newInfo, kSCDRegexData);
		newRData = CFArrayCreateMutableCopy(NULL, 0, rData);

		i = CFArrayGetFirstIndexOfValue(newRKeys,
						CFRangeMake(0, CFArrayGetCount(newRData)),
						key);
		regexData = CFArrayGetValueAtIndex(newRData, i);

		context.session = sessionPrivate;
		context.preg    = (regex_t *)CFDataGetBytePtr(regexData);
		CFDictionaryApplyFunction(cacheData,
					  (CFDictionaryApplierFunction)_removeRegexWatcherByKey,
					  &context);

		/* remove the regex key */
		CFArrayRemoveValueAtIndex(newRKeys, i);
		if (CFArrayGetCount(newRKeys) > 0) {
			CFDictionarySetValue(newInfo, kSCDRegexKeys, newRKeys);
		} else {
			CFDictionaryRemoveValue(newInfo, kSCDRegexKeys);
		}
		CFRelease(newRKeys);

		/* ...and the compiled expression */
		regfree((regex_t *)CFDataGetBytePtr(regexData));
		CFArrayRemoveValueAtIndex(newRData, i);
		if (CFArrayGetCount(newRData) > 0) {
			CFDictionarySetValue(newInfo, kSCDRegexData, newRData);
		} else {
			CFDictionaryRemoveValue(newInfo, kSCDRegexData);
		}
		CFRelease(newRData);

		/* save the updated session data */
		CFDictionarySetValue(sessionData, sessionKey, newInfo);
		CFRelease(newInfo);

		CFRelease(sessionKey);
	} else {
		CFNumberRef	sessionNum;

		/*
		 * We are watching a specific key. As such, update the
		 * cache to mark our interest in any changes.
		 */

		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionPrivate->server);
		_removeWatcher(sessionNum, key);
		CFRelease(sessionNum);
	}

	return SCD_OK;
}


kern_return_t
_notifyremove(mach_port_t		server,
	      xmlData_t			keyRef,		/* raw XML bytes */
	      mach_msg_type_number_t	keyLen,
	      int			regexOptions,
	      int			*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("Remove notification key for this session."));
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

	*scd_status = _SCDNotifierRemove(mySession->session, key, regexOptions);
	CFRelease(key);

	return KERN_SUCCESS;
}

