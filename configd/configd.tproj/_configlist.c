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

#define	N_QUICK	64

__private_extern__
int
__SCDynamicStoreCopyKeyList(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex, CFArrayRef *subKeys)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	CFMutableArrayRef		keyArray;
	regex_t				preg;
	CFIndex				storeCnt;
	CFStringRef			storeStr;
	CFDictionaryRef			storeValue;

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (isRegex) {
		CFStringRef	reErrStr;

		if (!patternCompile(key, &preg, &reErrStr)) {
			*subKeys = CFArrayCreate(NULL, (const void **)&reErrStr, 1, &kCFTypeArrayCallBacks);
			CFRelease(reErrStr);
			return kSCStatusFailed;
		}
	}

	storeCnt = CFDictionaryGetCount(storeData);
	keyArray = CFArrayCreateMutable(NULL, storeCnt, &kCFTypeArrayCallBacks);
	if (storeCnt > 0) {
		int		i;
		const void *	storeKeys_q[N_QUICK];
		const void **	storeKeys	= storeKeys_q;
		const void *	storeValues_q[N_QUICK];
		const void **	storeValues	= storeValues_q;

		if (storeCnt > (CFIndex)(sizeof(storeKeys_q) / sizeof(CFStringRef))) {
			storeKeys   = CFAllocatorAllocate(NULL, storeCnt * sizeof(CFStringRef), 0);
			storeValues = CFAllocatorAllocate(NULL, storeCnt * sizeof(CFStringRef), 0);
		}

		CFDictionaryGetKeysAndValues(storeData, storeKeys, storeValues);
		for (i = 0; i < storeCnt; i++) {
			storeStr   = (CFStringRef)storeKeys[i];
			storeValue = (CFDictionaryRef)storeValues[i];
			if (isRegex) {
				/*
				 * only return those keys which match the regular
				 * expression specified in the provided key.
				 */

				int	reError;
				char	storeKey_q[256];
				char *	storeKey	= storeKey_q;
				CFIndex	storeKeyLen	= CFStringGetLength(storeStr) + 1;

				if (storeKeyLen > (CFIndex)sizeof(storeKey_q))
					storeKey = CFAllocatorAllocate(NULL, storeKeyLen, 0);
				if (_SC_cfstring_to_cstring(storeStr, storeKey, storeKeyLen, kCFStringEncodingASCII) == NULL) {
					SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreCopyKeyList(): could not convert store key to C string"));
					if (storeKey != storeKey_q) CFAllocatorDeallocate(NULL, storeKey);
					continue;
				}

				reError = regexec(&preg, storeKey, 0, NULL, 0);
				switch (reError) {
					case 0 :
						/* we've got a match */
						if (CFDictionaryContainsKey(storeValue, kSCDData))
							CFArrayAppendValue(keyArray, storeStr);
						break;
					case REG_NOMATCH :
						/* no match */
						break;
					default : {
						char	reErrBuf[256];
						int	reErrStrLen;

						reErrStrLen = regerror(reError,
								       &preg,
								       reErrBuf,
								       sizeof(reErrBuf));
						SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreCopyKeyList regexec() failed: %s"), reErrBuf);
						break;
					}
				}
				if (storeKey != storeKey_q) CFAllocatorDeallocate(NULL, storeKey);
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

		if (storeKeys != storeKeys_q) {
			CFAllocatorDeallocate(NULL, storeKeys);
			CFAllocatorDeallocate(NULL, storeValues);
		}
	}

	if (isRegex) {
		regfree(&preg);
	}

	*subKeys = CFArrayCreateCopy(NULL, keyArray);
	CFRelease(keyArray);
	return kSCStatusOK;
}


__private_extern__
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
	CFStringRef		key		= NULL;		/* key  (un-serialized) */
	serverSessionRef	mySession	= getSession(server);
	Boolean			ok;
	CFArrayRef		subKeys;			/* array of CFStringRef's */

	*listRef = NULL;
	*listLen = 0;

	/* un-serialize the key */
	if (!_SCUnserializeString(&key, NULL, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
		goto done;
	}

	if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	if (!mySession) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		goto done;
	}

	*sc_status = __SCDynamicStoreCopyKeyList(mySession->store, key, isRegex != 0, &subKeys);
	if (*sc_status != kSCStatusOK) {
		goto done;
	}

	/* serialize the list of keys */
	ok = _SCSerialize(subKeys, NULL, (void **)listRef, (CFIndex *)listLen);
	CFRelease(subKeys);
	if (!ok) {
		*sc_status = kSCStatusFailed;
		goto done;
	}

    done :

	if (key)	CFRelease(key);
	return KERN_SUCCESS;
}
