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
 * January 8, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

CFStringRef
SCDynamicStoreKeyCreateComputerName(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreate(allocator,
				       CFSTR("%@/%@"),
				       kSCDynamicStoreDomainSetup,
				       kSCCompSystem);
}


CFStringRef
SCDynamicStoreCopyComputerName(SCDynamicStoreRef	store,
			       CFStringEncoding		*nameEncoding)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		name		= NULL;
	Boolean			tempSession	= FALSE;

	if (!store) {
		store = SCDynamicStoreCreate(NULL,
					     CFSTR("SCDynamicStoreCopyComputerName"),
					     NULL,
					     NULL);
		if (!store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
		tempSession = TRUE;
	}

	key  = SCDynamicStoreKeyCreateComputerName(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (!dict) {
		goto done;
	}
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	name = isA_CFString(CFDictionaryGetValue(dict, kSCPropSystemComputerName));
	if (!name) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}
	CFRetain(name);

	if (nameEncoding) {
		CFNumberRef	num;

		num = CFDictionaryGetValue(dict,
					   kSCPropSystemComputerNameEncoding);
		if (isA_CFNumber(num)) {
			CFNumberGetValue(num, kCFNumberIntType, nameEncoding);
		} else {
			*nameEncoding = CFStringGetSystemEncoding();
		}
	}

    done :

	if (tempSession)	CFRelease(store);
	if (dict)		CFRelease(dict);
	return name;
}


Boolean
SCPreferencesSetComputerName(SCPreferencesRef	session,
			     CFStringRef	name,
			     CFStringEncoding	encoding)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict	= NULL;
	CFNumberRef		num;
	Boolean			ok	= FALSE;
	CFStringRef		path	= NULL;

	if (!isA_CFString(name)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefSystem,
					kSCCompSystem);

	dict = SCPreferencesPathGetValue(session, path);
	if (dict) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	CFDictionarySetValue(newDict, kSCPropSystemComputerName, name);

	num = CFNumberCreate(NULL, kCFNumberIntType, &encoding);
	CFDictionarySetValue(newDict, kSCPropSystemComputerNameEncoding, num);
	CFRelease(num);

	ok = SCPreferencesPathSetValue(session, path, newDict);
	if (!ok) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesPathSetValue() failed"));
	}

	if (path)	CFRelease(path);
	if (newDict)	CFRelease(newDict);

	return ok;
}


CFStringRef
SCDynamicStoreKeyCreateHostNames(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreate(allocator,
				       CFSTR("%@/%@/%@"),
				       kSCDynamicStoreDomainSetup,
				       kSCCompNetwork,
				       kSCCompHostNames);
}


CFStringRef
SCDynamicStoreCopyLocalHostName(SCDynamicStoreRef store)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		name		= NULL;
	Boolean			tempSession	= FALSE;

	if (!store) {
		store = SCDynamicStoreCreate(NULL,
					     CFSTR("SCDynamicStoreCopyLocalHostName"),
					     NULL,
					     NULL);
		if (!store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
		tempSession = TRUE;
	}

	key  = SCDynamicStoreKeyCreateHostNames(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (!dict) {
		goto done;
	}
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	name = isA_CFString(CFDictionaryGetValue(dict, kSCPropNetLocalHostName));
	if (!name) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}
	CFRetain(name);

    done :

	if (tempSession)	CFRelease(store);
	if (dict)		CFRelease(dict);
	return name;
}


Boolean
_SC_stringIsValidDNSName(const char *name)
{
	int		i;
	int		len	= strlen(name);
	char		prev	= '\0';
	const char	*scan;

	if (len == 0) {
		return FALSE;
	}

	for (scan = name, i = 0; i < len; i++, scan++) {
		char	ch	= *scan;
		char 	next	= *(scan + 1);

		if (prev == '.' || prev == '\0') {
			if (isalpha(ch) == 0) {
				return FALSE;
			}
		} else if (next == '\0' || next == '.') {
			if (isalnum(ch) == 0) {
				return FALSE;
			}
		} else if (isalnum(ch) == 0) {
			switch (ch) {
				case '.':
				case '-':
					if (prev == '.' || prev == '-') {
						return FALSE;
					}
					break;
				default:
					return FALSE;
					break;
			}
		}
		prev = ch;
	}

	return TRUE;
}

Boolean
_SC_CFStringIsValidDNSName(CFStringRef name)
{
	Boolean	clean	= FALSE;
	CFIndex	len;
	char	*str	= NULL;

	if (!isA_CFString(name)) {
		goto failed;
	}

	len = CFStringGetLength(name) + 1;
	if (len == 0) {
		goto failed;
	}

	str = CFAllocatorAllocate(NULL, len, 0);
	if (str == NULL) {
		goto failed;
	}

	if (!CFStringGetCString(name, str, len, kCFStringEncodingASCII)) {
		goto failed;
	}

	clean = _SC_stringIsValidDNSName(str);

    failed:

	if (str)	CFAllocatorDeallocate(NULL, str);
	return clean;
}


Boolean
SCPreferencesSetLocalHostName(SCPreferencesRef	session,
			     CFStringRef	name)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict	= NULL;
	Boolean			ok	= FALSE;
	CFStringRef		path	= NULL;

	if (name) {
		if (!isA_CFString(name)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}

		if (CFStringGetLength(name) == 0) {
			name = NULL;
		}
	}

	if (name && !_SC_CFStringIsValidDNSName(name)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@/%@"),
					kSCPrefSystem,
					kSCCompNetwork,
					kSCCompHostNames);

	dict = SCPreferencesPathGetValue(session, path);
	if (dict) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	if (name) {
		CFDictionarySetValue(newDict, kSCPropNetLocalHostName, name);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetLocalHostName);
	}

	if (CFDictionaryGetCount(newDict) > 0) {
		ok = SCPreferencesPathSetValue(session, path, newDict);
		if (!ok) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesPathSetValue() failed"));
		}
	} else {
		ok = SCPreferencesPathRemoveValue(session, path);
		if (!ok) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesPathRemoveValue() failed"));
		}
	}

	if (path)	CFRelease(path);
	if (newDict)	CFRelease(newDict);

	return ok;
}
