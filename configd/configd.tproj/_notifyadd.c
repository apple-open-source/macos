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


static __inline__ void
my_CFDictionaryApplyFunction(CFDictionaryRef			theDict,
			     CFDictionaryApplierFunction	applier,
			     void				*context)
{
	CFAllocatorRef	myAllocator;
	CFDictionaryRef	myDict;

	myAllocator = CFGetAllocator(theDict);
	myDict      = CFDictionaryCreateCopy(myAllocator, theDict);
	CFDictionaryApplyFunction(myDict, applier, context);
	CFRelease(myDict);
	return;
}


int
__SCDynamicStoreAddWatchedKey(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreAddWatchedKey:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key     = %@"), key);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  isRegex = %s"), isRegex ? "TRUE" : "FALSE");

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	/*
	 * add new key after checking if key has already been defined
	 */
	if (isRegex) {
		if (CFSetContainsValue(storePrivate->reKeys, key))
			return kSCStatusKeyExists;		/* sorry, key already exists in notifier list */
		CFSetAddValue(storePrivate->reKeys, key);	/* add key to this sessions notifier list */
	} else {
		if (CFSetContainsValue(storePrivate->keys, key))
			return kSCStatusKeyExists;		/* sorry, key already exists in notifier list */
		CFSetAddValue(storePrivate->keys, key);	/* add key to this sessions notifier list */
	}

	if (isRegex) {
		CFStringRef		sessionKey;
		int			regexStrLen;
		char			*regexStr;
		CFMutableDataRef	regexData;
		int			reError;
		char			reErrBuf[256];
		int			reErrStrLen;
		addContext		context;
		CFDictionaryRef		info;
		CFMutableDictionaryRef	newInfo;
		CFArrayRef		rKeys;
		CFMutableArrayRef	newRKeys;
		CFArrayRef		rData;
		CFMutableArrayRef	newRData;

		/*
		 * We are adding a regex key. As such, we need to flag
		 * any keys currently in the store.
		 */

		/* 1. Extract a C String version of the key pattern string. */

		regexStrLen = CFStringGetLength(key) + 1;
		regexStr    = CFAllocatorAllocate(NULL, regexStrLen, 0);
		if (!CFStringGetCString(key,
					regexStr,
					regexStrLen,
					kCFStringEncodingMacRoman)) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString: could not convert regex key to C string"));
			CFAllocatorDeallocate(NULL, regexStr);
			return kSCStatusFailed;
		}

		/* 2. Compile the regular expression from the pattern string. */

		regexData = CFDataCreateMutable(NULL, sizeof(regex_t));
		CFDataSetLength(regexData, sizeof(regex_t));
		reError = regcomp((regex_t *)CFDataGetBytePtr(regexData),
				  regexStr,
				  REG_EXTENDED);
		CFAllocatorDeallocate(NULL, regexStr);
		if (reError != 0) {
			reErrStrLen = regerror(reError,
					       (regex_t *)CFDataGetBytePtr(regexData),
					       reErrBuf,
					       sizeof(reErrBuf));
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("regcomp() key: %s"), reErrBuf);
			CFRelease(regexData);
			return kSCStatusFailed;
		}

		/*
		 * 3. Iterate over the current keys and add this session as a "watcher"
		 *    for any key already defined in the store.
		 */

		context.store = storePrivate;
		context.preg  = (regex_t *)CFDataGetBytePtr(regexData);
		my_CFDictionaryApplyFunction(storeData,
					     (CFDictionaryApplierFunction)_addRegexWatcherByKey,
					     &context);

		/*
		 * 4. We also need to save this key and the associated regex data
		 *    for any subsequent additions.
		 */
		sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);

		info = CFDictionaryGetValue(sessionData, sessionKey);
		if (info) {
			newInfo = CFDictionaryCreateMutableCopy(NULL, 0, info);
		} else {
			newInfo = CFDictionaryCreateMutable(NULL,
							    0,
							    &kCFTypeDictionaryKeyCallBacks,
							    &kCFTypeDictionaryValueCallBacks);
		}

		rKeys = CFDictionaryGetValue(newInfo, kSCDRegexKeys);
		if ((rKeys == NULL) ||
		    (CFArrayContainsValue(rKeys,
					  CFRangeMake(0, CFArrayGetCount(rKeys)),
					  key) == FALSE)) {
			rData = CFDictionaryGetValue(newInfo, kSCDRegexData);
			if (rKeys) {
				newRKeys = CFArrayCreateMutableCopy(NULL, 0, rKeys);
				newRData = CFArrayCreateMutableCopy(NULL, 0, rData);
			} else {
				newRKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				newRData = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}

			/* save the regex key */
			CFArrayAppendValue(newRKeys, key);
			CFDictionarySetValue(newInfo, kSCDRegexKeys, newRKeys);
			CFRelease(newRKeys);
			/* ...and the compiled expression */
			CFArrayAppendValue(newRData, regexData);
			CFDictionarySetValue(newInfo, kSCDRegexData, newRData);
			CFRelease(newRData);
			CFDictionarySetValue(sessionData, sessionKey, newInfo);
		}
		CFRelease(regexData);
		CFRelease(newInfo);
		CFRelease(sessionKey);
	} else {
		CFNumberRef	sessionNum;

		/*
		 * We are watching a specific key. As such, update the
		 * store to mark our interest in any changes.
		 */
		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);
		_addWatcher(sessionNum, key);
		CFRelease(sessionNum);
	}

	return kSCStatusOK;
}


kern_return_t
_notifyadd(mach_port_t 			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   int				isRegex,
	   int				*sc_status
)
{
	serverSessionRef	mySession = getSession(server);
	CFStringRef		key;		/* key  (un-serialized) */

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Add notification key for this session."));
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

	*sc_status = __SCDynamicStoreAddWatchedKey(mySession->store, key, isRegex);
	CFRelease(key);

	return KERN_SUCCESS;
}
