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

int
__SCDynamicStoreCopyKeyList(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex, CFArrayRef *subKeys)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	CFIndex				storeCnt;
	const void			**storeKeys;
	const void			**storeValues;
	CFMutableArrayRef		keyArray;
	int				i;
	CFStringRef			storeStr;
	CFDictionaryRef			storeValue;
	int				regexBufLen;
	char				*regexBuf = NULL;
	regex_t				preg;
	int				reError;
	char				reErrBuf[256];
	int				reErrStrLen;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreCopyKeyList:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key     = %@"), key);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  isRegex = %s"), isRegex ? "TRUE" : "FALSE");

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	storeCnt = CFDictionaryGetCount(storeData);
	keyArray = CFArrayCreateMutable(NULL, storeCnt, &kCFTypeArrayCallBacks);

	if (isRegex) {
		UniChar			ch_s	= 0;
		UniChar			ch_e	= 0;
		Boolean			ok;
		CFIndex			regexLen;
		CFMutableStringRef	regexStr;

		regexStr = CFStringCreateMutableCopy(NULL, 0, key);
		regexLen = CFStringGetLength(regexStr);
		if (regexLen > 0) {
			ch_s = CFStringGetCharacterAtIndex(regexStr, 0);
			ch_e = CFStringGetCharacterAtIndex(regexStr, regexLen - 1);
		}
		if ((regexLen == 0) || ((ch_s != (UniChar)'^') && (ch_e != (UniChar)'$'))) {
			/* if regex pattern is not already bounded */
			CFStringInsert(regexStr, 0, CFSTR("^"));
			CFStringAppend(regexStr,    CFSTR("$"));
		}

		/*
		 * compile the provided regular expression using the
		 * provided isRegex.
		 */
		regexBufLen = CFStringGetLength(regexStr) + 1;
		regexBuf    = CFAllocatorAllocate(NULL, regexBufLen, 0);
		ok = CFStringGetCString(regexStr, regexBuf, regexBufLen, kCFStringEncodingMacRoman);
		CFRelease(regexStr);
		if (!ok) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString() key: could not convert to regex string"));
			CFAllocatorDeallocate(NULL, regexBuf);
			return kSCStatusFailed;
		}

		reError = regcomp(&preg, regexBuf, REG_EXTENDED);
		if (reError != 0) {
			reErrStrLen = regerror(reError, &preg, reErrBuf, sizeof(reErrBuf));
			storeStr = CFStringCreateWithCString(NULL, reErrBuf, kCFStringEncodingMacRoman);
			CFArrayAppendValue(keyArray, storeStr);
			CFRelease(storeStr);
			*subKeys = CFArrayCreateCopy(NULL, keyArray);
			CFRelease(keyArray);
			CFAllocatorDeallocate(NULL, regexBuf);
			return kSCStatusFailed;
		}
	}

	if (storeCnt > 0) {
		storeKeys   = CFAllocatorAllocate(NULL, storeCnt * sizeof(CFStringRef), 0);
		storeValues = CFAllocatorAllocate(NULL, storeCnt * sizeof(CFStringRef), 0);
		CFDictionaryGetKeysAndValues(storeData, storeKeys, storeValues);
		for (i=0; i<storeCnt; i++) {
			storeStr   = (CFStringRef)storeKeys[i];
			storeValue = (CFDictionaryRef)storeValues[i];
			if (isRegex) {
				/*
				 * only return those keys which match the regular
				 * expression specified in the provided key.
				 */

				int	storeKeyLen = CFStringGetLength(storeStr) + 1;
				char	*storeKey   = CFAllocatorAllocate(NULL, storeKeyLen, 0);

				if (!CFStringGetCString(storeStr,
							storeKey,
							storeKeyLen,
							kCFStringEncodingMacRoman)) {
					SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString: could not convert store key to C string"));
					CFAllocatorDeallocate(NULL, storeKey);
					continue;
				}

				reError = regexec(&preg,
						  storeKey,
						  0,
						  NULL,
						  0);
				switch (reError) {
					case 0 :
						/* we've got a match */
						if (CFDictionaryContainsKey(storeValue, kSCDData))
							CFArrayAppendValue(keyArray, storeStr);
						break;
					case REG_NOMATCH :
						/* no match */
						break;
					default :
						reErrStrLen = regerror(reError,
								       &preg,
								       reErrBuf,
								       sizeof(reErrBuf));
						SCLog(_configd_verbose, LOG_DEBUG, CFSTR("regexec(): %s"), reErrBuf);
						break;
				}
				CFAllocatorDeallocate(NULL, storeKey);
			} else {
				/*
				 * only return those keys which are prefixed by the
				 * provided key string and have data.
				 */
				if (((CFStringGetLength(key) == 0) || CFStringHasPrefix(storeStr, key)) &&
				    CFDictionaryContainsKey(storeValue, kSCDData)) {
					CFArrayAppendValue(keyArray, storeStr);
				}
			}
		}
		CFAllocatorDeallocate(NULL, storeKeys);
		CFAllocatorDeallocate(NULL, storeValues);
	}

	if (isRegex) {
		regfree(&preg);
		CFAllocatorDeallocate(NULL, regexBuf);
	}

	*subKeys = keyArray;

	return kSCStatusOK;
}


kern_return_t
_configlist(mach_port_t			server,
	    xmlData_t			keyRef,		/* raw XML bytes */
	    mach_msg_type_number_t	keyLen,
	    int				isRegex,
	    xmlDataOut_t		*listRef,	/* raw XML bytes */
	    mach_msg_type_number_t	*listLen,
	    int				*sc_status
)
{
	CFStringRef		key;		/* key  (un-serialized) */
	serverSessionRef	mySession = getSession(server);
	Boolean			ok;
	CFArrayRef		subKeys;	/* array of CFStringRef's */

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("List keys in configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*listRef = NULL;
	*listLen = 0;

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

	*sc_status = __SCDynamicStoreCopyKeyList(mySession->store, key, isRegex, &subKeys);
	CFRelease(key);
	if (*sc_status != kSCStatusOK) {
		return KERN_SUCCESS;
	}

	/* serialize the list of keys */
	ok = _SCSerialize(subKeys, NULL, (void **)listRef, (CFIndex *)listLen);
	CFRelease(subKeys);
	if (!ok) {
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	return KERN_SUCCESS;
}
