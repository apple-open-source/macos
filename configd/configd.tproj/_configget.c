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
__SCDynamicStoreCopyValue(SCDynamicStoreRef store, CFStringRef key, CFPropertyListRef *value)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	CFDictionaryRef			dict;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreCopyValue:"));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  key      = %@"), key);

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	dict = CFDictionaryGetValue(storeData, key);
	if ((dict == NULL) || (CFDictionaryContainsKey(dict, kSCDData) == FALSE)) {
		/* key doesn't exist (or data never defined) */
		return kSCStatusNoKey;
	}

	/* Return the data associated with the key */
	*value = CFRetain(CFDictionaryGetValue(dict, kSCDData));

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  value    = %@"), *value);

	return kSCStatusOK;
}

kern_return_t
_configget(mach_port_t			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   xmlDataOut_t			*dataRef,	/* raw XML bytes */
	   mach_msg_type_number_t	*dataLen,
	   int				*newInstance,
	   int				*sc_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		xmlData;	/* data (XML serialized) */
	CFPropertyListRef	value;
	CFStringRef		xmlError;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Get key from configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

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
		return KERN_SUCCESS;
	} else if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreCopyValue(mySession->store, key, &value);
	CFRelease(key);
	if (*sc_status != kSCStatusOK) {
		*dataRef = NULL;
		*dataLen = 0;
		return KERN_SUCCESS;
	}

	/*
	 * serialize the data, copy it into an allocated buffer which will be
	 * released when it is returned as part of a Mach message.
	 */
	xmlData = CFPropertyListCreateXMLData(NULL, value);
	CFRelease(value);
	*dataLen = CFDataGetLength(xmlData);
	status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_allocate(): %s"), mach_error_string(status));
		*sc_status = kSCStatusFailed;
		CFRelease(xmlData);
		*dataRef = NULL;
		*dataLen = 0;
		return KERN_SUCCESS;
	}

	bcopy((char *)CFDataGetBytePtr(xmlData), *dataRef, *dataLen);
	CFRelease(xmlData);

	/*
	 * return the instance number associated with the returned data.
	 */
	*newInstance = 1;

	return KERN_SUCCESS;
}

/*
 * "context" argument for addSpecificKey() and addSpecificPattern()
 */
typedef struct {
	SCDynamicStoreRef	store;
	CFMutableDictionaryRef	dict;
} addSpecific, *addSpecificRef;

static void
addSpecificKey(const void *value, void *context)
{
	CFStringRef		key		= (CFStringRef)value;
	addSpecificRef		myContextRef	= (addSpecificRef)context;
	int			sc_status;
	CFPropertyListRef	data;

	if (!isA_CFString(key)) {
		return;
	}

	sc_status = __SCDynamicStoreCopyValue(myContextRef->store, key, &data);
	if (sc_status == kSCStatusOK) {
		CFDictionaryAddValue(myContextRef->dict, key, data);
		CFRelease(data);
	}

	return;
}

static void
addSpecificPattern(const void *value, void *context)
{
	CFStringRef		pattern		= (CFStringRef)value;
	addSpecificRef		myContextRef	= (addSpecificRef)context;
	int			sc_status;
	CFArrayRef		keys;

	if (!isA_CFString(pattern)) {
		return;
	}

	sc_status = __SCDynamicStoreCopyKeyList(myContextRef->store, pattern, TRUE, &keys);
	if (sc_status == kSCStatusOK) {
		CFArrayApplyFunction(keys,
				     CFRangeMake(0, CFArrayGetCount(keys)),
				     addSpecificKey,
				     context);
		CFRelease(keys);
	}

	return;
}

kern_return_t
_configget_m(mach_port_t		server,
	     xmlData_t			keysRef,
	     mach_msg_type_number_t	keysLen,
	     xmlData_t			patternsRef,
	     mach_msg_type_number_t	patternsLen,
	     xmlDataOut_t		*dataRef,
	     mach_msg_type_number_t	*dataLen,
	     int			*sc_status)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFArrayRef		keys		= NULL;	/* keys (un-serialized) */
	CFArrayRef		patterns	= NULL;	/* patterns (un-serialized) */
	CFDataRef		xmlData;		/* data (XML serialized) */
	addSpecific		myContext;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Get key from configuration database."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*sc_status = kSCStatusOK;

	if (keysRef && (keysLen > 0)) {
		CFDataRef	xmlKeys;		/* keys (XML serialized) */
		CFStringRef	xmlError;

		/* un-serialize the keys */
		xmlKeys = CFDataCreate(NULL, keysRef, keysLen);
		status = vm_deallocate(mach_task_self(), (vm_address_t)keysRef, keysLen);
		if (status != KERN_SUCCESS) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		keys = CFPropertyListCreateFromXMLData(NULL,
						       xmlKeys,
						       kCFPropertyListImmutable,
						       &xmlError);
		CFRelease(xmlKeys);
		if (!keys) {
			if (xmlError) {
				SCLog(_configd_verbose, LOG_DEBUG,
				       CFSTR("CFPropertyListCreateFromXMLData() keys: %@"),
				       xmlError);
				CFRelease(xmlError);
			}
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(keys)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (patternsRef && (patternsLen > 0)) {
		CFDataRef	xmlPatterns;		/* patterns (XML serialized) */
		CFStringRef	xmlError;

		/* un-serialize the patterns */
		xmlPatterns = CFDataCreate(NULL, patternsRef, patternsLen);
		status = vm_deallocate(mach_task_self(), (vm_address_t)patternsRef, patternsLen);
		if (status != KERN_SUCCESS) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		patterns = CFPropertyListCreateFromXMLData(NULL,
							   xmlPatterns,
							   kCFPropertyListImmutable,
							   &xmlError);
		CFRelease(xmlPatterns);
		if (!patterns) {
			if (xmlError) {
				SCLog(_configd_verbose, LOG_DEBUG,
				       CFSTR("CFPropertyListCreateFromXMLData() patterns: %@"),
				       xmlError);
				CFRelease(xmlError);
			}
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(patterns)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (*sc_status != kSCStatusOK) {
		if (keys)	CFRelease(keys);
		if (patterns)	CFRelease(patterns);
		*dataRef = NULL;
		*dataLen = 0;
		return KERN_SUCCESS;
	}

	myContext.store = mySession->store;
	myContext.dict  = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);

	if (keys) {
		CFArrayApplyFunction(keys,
				     CFRangeMake(0, CFArrayGetCount(keys)),
				     addSpecificKey,
				     &myContext);
		CFRelease(keys);
	}

	if (patterns) {
		CFArrayApplyFunction(patterns,
				     CFRangeMake(0, CFArrayGetCount(patterns)),
				     addSpecificPattern,
				     &myContext);
		CFRelease(patterns);
	}

	/*
	 * serialize the dictionary of matching keys/patterns, copy it into an
	 * allocated buffer which will be released when it is returned as part
	 * of a Mach message.
	 */
	xmlData = CFPropertyListCreateXMLData(NULL, myContext.dict);
	CFRelease(myContext.dict);
	*dataLen = CFDataGetLength(xmlData);
	status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_allocate(): %s"), mach_error_string(status));
		*sc_status = kSCStatusFailed;
		CFRelease(xmlData);
		*dataRef = NULL;
		*dataLen = 0;
		return KERN_SUCCESS;
	}

	bcopy((char *)CFDataGetBytePtr(xmlData), *dataRef, *dataLen);
	CFRelease(xmlData);

	return KERN_SUCCESS;
}
