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
_SCDNotifierAdd(SCDSessionRef session, CFStringRef key, int regexOptions)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;

	SCDLog(LOG_DEBUG, CFSTR("_SCDNotifierAdd:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  regexOptions = %0o"), regexOptions);

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/*
	 * add new key after checking if key has already been defined
	 */
	if (regexOptions & kSCDRegexKey) {
		if (CFSetContainsValue(sessionPrivate->reKeys, key))
			return SCD_EXISTS;		/* sorry, key already exists in notifier list */
		CFSetAddValue(sessionPrivate->reKeys, key);	/* add key to this sessions notifier list */
	} else {
		if (CFSetContainsValue(sessionPrivate->keys, key))
			return SCD_EXISTS;		/* sorry, key already exists in notifier list */
		CFSetAddValue(sessionPrivate->keys, key);	/* add key to this sessions notifier list */
	}

	if (regexOptions & kSCDRegexKey) {
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
		 * any keys currently in the cache.
		 */

		/* 1. Extract a C String version of the key pattern string. */

		regexStrLen = CFStringGetLength(key) + 1;
		regexStr    = CFAllocatorAllocate(NULL, regexStrLen, 0);
		if (!CFStringGetCString(key,
					regexStr,
					regexStrLen,
					kCFStringEncodingMacRoman)) {
			SCDLog(LOG_DEBUG, CFSTR("CFStringGetCString: could not convert regex key to C string"));
			CFAllocatorDeallocate(NULL, regexStr);
			return SCD_FAILED;
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
			SCDLog(LOG_DEBUG, CFSTR("regcomp() key: %s"), reErrBuf);
			CFRelease(regexData);
			return SCD_FAILED;
		}

		/*
		 * 3. Iterate over the current keys and add this session as a "watcher"
		 *    for any key already defined in the cache.
		 */

		context.session = sessionPrivate;
		context.preg    = (regex_t *)CFDataGetBytePtr(regexData);
		CFDictionaryApplyFunction(cacheData,
					  (CFDictionaryApplierFunction)_addRegexWatcherByKey,
					  &context);

		/*
		 * 4. We also need to save this key and the associated regex data
		 *    for any subsequent additions.
		 */
		sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);

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
		CFRelease(newInfo);
		CFRelease(sessionKey);
	} else {
		CFNumberRef	sessionNum;

		/*
		 * We are watching a specific key. As such, update the
		 * cache to mark our interest in any changes.
		 */
		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionPrivate->server);
		_addWatcher(sessionNum, key);
		CFRelease(sessionNum);
	}

	return SCD_OK;
}


kern_return_t
_notifyadd(mach_port_t 			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   int				regexOptions,
	   int				*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("Add notification key for this session."));
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

	*scd_status = _SCDNotifierAdd(mySession->session, key, regexOptions);
	CFRelease(key);

	return KERN_SUCCESS;
}
