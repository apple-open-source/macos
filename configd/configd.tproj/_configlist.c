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
_SCDList(SCDSessionRef session, CFStringRef key, int regexOptions, CFArrayRef *subKeys)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	CFIndex			cacheCnt;
	void			**cacheKeys;
	void			**cacheValues;
	CFMutableArrayRef	keyArray;
	int			i;
	CFStringRef		cacheStr;
	CFDictionaryRef		cacheValue;
	int			regexStrLen;
	char			*regexStr = NULL;
	regex_t			preg;
	int			reError;
	char			reErrBuf[256];
	int			reErrStrLen;

	SCDLog(LOG_DEBUG, CFSTR("_SCDList:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  regexOptions = %0o"), regexOptions);

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	cacheCnt = CFDictionaryGetCount(cacheData);
	keyArray = CFArrayCreateMutable(NULL, cacheCnt, &kCFTypeArrayCallBacks);

	if (regexOptions & kSCDRegexKey) {
		/*
		 * compile the provided regular expression using the
		 * provided regexOptions (passing only those flags
		 * which would make sense).
		 */
		regexStrLen = CFStringGetLength(key) + 1;
		regexStr    = CFAllocatorAllocate(NULL, regexStrLen, 0);
		if (!CFStringGetCString(key,
					regexStr,
					regexStrLen,
					kCFStringEncodingMacRoman)) {
			SCDLog(LOG_DEBUG, CFSTR("CFStringGetCString() key: could not convert to regex string"));
			CFAllocatorDeallocate(NULL, regexStr);
			return SCD_FAILED;
		}

		reError = regcomp(&preg, regexStr, REG_EXTENDED);
		if (reError != 0) {
			reErrStrLen = regerror(reError, &preg, reErrBuf, sizeof(reErrBuf));
			cacheStr = CFStringCreateWithCString(NULL, reErrBuf, kCFStringEncodingMacRoman);
			CFArrayAppendValue(keyArray, cacheStr);
			CFRelease(cacheStr);
			*subKeys = CFArrayCreateCopy(NULL, keyArray);
			CFRelease(keyArray);
			CFAllocatorDeallocate(NULL, regexStr);
			return SCD_FAILED;
		}
	}

	cacheKeys   = CFAllocatorAllocate(NULL, cacheCnt * sizeof(CFStringRef), 0);
	cacheValues = CFAllocatorAllocate(NULL, cacheCnt * sizeof(CFStringRef), 0);
	CFDictionaryGetKeysAndValues(cacheData, cacheKeys, cacheValues);
	for (i=0; i<cacheCnt; i++) {
		cacheStr   = (CFStringRef)cacheKeys[i];
		cacheValue = (CFDictionaryRef)cacheValues[i];
		if (regexOptions & kSCDRegexKey) {
			/*
			 * only return those keys which match the regular
			 * expression specified in the provided key.
			 */

			int	cacheKeyLen = CFStringGetLength(cacheStr) + 1;
			char	*cacheKey   = CFAllocatorAllocate(NULL, cacheKeyLen, 0);

			if (!CFStringGetCString(cacheStr,
						cacheKey,
						cacheKeyLen,
						kCFStringEncodingMacRoman)) {
				SCDLog(LOG_DEBUG, CFSTR("CFStringGetCString: could not convert cache key to C string"));
				CFAllocatorDeallocate(NULL, cacheKey);
				continue;
			}

			reError = regexec(&preg,
					  cacheKey,
					  0,
					  NULL,
					  0);
			switch (reError) {
				case 0 :
					/* we've got a match */
					if (CFDictionaryContainsKey(cacheValue, kSCDData))
						CFArrayAppendValue(keyArray, cacheStr);
					break;
				case REG_NOMATCH :
					/* no match */
					break;
				default :
					reErrStrLen = regerror(reError,
							       &preg,
							       reErrBuf,
							       sizeof(reErrBuf));
					SCDLog(LOG_DEBUG, CFSTR("regexec(): %s"), reErrBuf);
					break;
			}
			CFAllocatorDeallocate(NULL, cacheKey);
		} else {
			/*
			 * only return those keys which are prefixed by the
			 * provided key string and have data.
			 */
			if (((CFStringGetLength(key) == 0) || CFStringHasPrefix(cacheStr, key)) &&
			    CFDictionaryContainsKey(cacheValue, kSCDData)) {
				CFArrayAppendValue(keyArray, cacheStr);
			}
		}
	}
	CFAllocatorDeallocate(NULL, cacheKeys);
	CFAllocatorDeallocate(NULL, cacheValues);

	if (regexOptions & kSCDRegexKey) {
		regfree(&preg);
	}

	*subKeys = keyArray;

	if (regexOptions & kSCDRegexKey) {
		CFAllocatorDeallocate(NULL, regexStr);
	}

	return SCD_OK;
}


kern_return_t
_configlist(mach_port_t			server,
	    xmlData_t			keyRef,		/* raw XML bytes */
	    mach_msg_type_number_t	keyLen,
	    int				regexOptions,
	    xmlDataOut_t		*listRef,	/* raw XML bytes */
	    mach_msg_type_number_t	*listLen,
	    int				*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFArrayRef		subKeys;	/* array of CFStringRef's */
	CFDataRef		xmlList;	/* list (XML serialized) */
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("List keys in configuration database."));
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

	*scd_status = _SCDList(mySession->session, key, regexOptions, &subKeys);
	CFRelease(key);
	if (*scd_status != SCD_OK) {
		*listRef = NULL;
		*listLen = 0;
		return KERN_SUCCESS;
	}

	/*
	 * serialize the array, copy it into an allocated buffer which will be
	 * released when it is returned as part of a Mach message.
	 */
	xmlList = CFPropertyListCreateXMLData(NULL, subKeys);
	CFRelease(subKeys);
	*listLen = CFDataGetLength(xmlList);
	status = vm_allocate(mach_task_self(), (void *)listRef, *listLen, TRUE);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_allocate(): %s"), mach_error_string(status));
		*scd_status = SCD_FAILED;
		CFRelease(xmlList);
		*listRef = NULL;
		*listLen = 0;
		return KERN_SUCCESS;
	}
	bcopy((char *)CFDataGetBytePtr(xmlList), *listRef, *listLen);
	CFRelease(xmlList);

	return KERN_SUCCESS;
}
