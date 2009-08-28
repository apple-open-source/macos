/*
 * Copyright (c) 2000-2005, 2009 Apple Inc. All rights reserved.
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

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */


CFDictionaryRef
SCDynamicStoreCopyMultiple(SCDynamicStoreRef	store,
			   CFArrayRef		keys,
			   CFArrayRef		patterns)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			xmlKeys		= NULL;	/* keys (XML serialized) */
	xmlData_t			myKeysRef	= NULL;	/* keys (serialized) */
	CFIndex				myKeysLen	= 0;
	CFDataRef			xmlPatterns	= NULL;	/* patterns (XML serialized) */
	xmlData_t			myPatternsRef	= NULL;	/* patterns (serialized) */
	CFIndex				myPatternsLen	= 0;
	xmlDataOut_t			xmlDictRef;		/* dict (serialized) */
	mach_msg_type_number_t		xmlDictLen;
	CFDictionaryRef			dict		= NULL;	/* dict (un-serialized) */
	CFDictionaryRef			expDict		= NULL;	/* dict (un-serialized / expanded) */
	int				sc_status;

	if (store == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;	/* you must have an open session to play */
	}

	/* serialize the keys */
	if (keys) {
		if (!_SCSerialize(keys, &xmlKeys, (void **)&myKeysRef, &myKeysLen)) {
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	/* serialize the patterns */
	if (patterns) {
		if (!_SCSerialize(patterns, &xmlPatterns, (void **)&myPatternsRef, &myPatternsLen)) {
			CFRelease(xmlKeys);
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	/* send the keys and patterns, fetch the associated result from the server */
	status = configget_m(storePrivate->server,
			     myKeysRef,
			     myKeysLen,
			     myPatternsRef,
			     myPatternsLen,
			     &xmlDictRef,
			     &xmlDictLen,
			     (int *)&sc_status);

	/* clean up */
	if (xmlKeys)		CFRelease(xmlKeys);
	if (xmlPatterns)	CFRelease(xmlPatterns);

	if (status != KERN_SUCCESS) {
		if (status == MACH_SEND_INVALID_DEST) {
			/* the server's gone and our session port's dead, remove the dead name right */
			(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
		} else {
			/* we got an unexpected error, leave the [session] port alone */
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreCopyMultiple configget_m(): %s"), mach_error_string(status));
		}
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		(void) vm_deallocate(mach_task_self(), (vm_address_t)xmlDictRef, xmlDictLen);
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the dictionary */
	if (!_SCUnserialize((CFPropertyListRef *)&dict, NULL, xmlDictRef, xmlDictLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	expDict = _SCUnserializeMultiple(dict);
	CFRelease(dict);

	return expDict;
}


CFPropertyListRef
SCDynamicStoreCopyValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			utfKey;		/* key (XML serialized) */
	xmlData_t			myKeyRef;	/* key (serialized) */
	CFIndex				myKeyLen;
	xmlDataOut_t			xmlDataRef;	/* data (serialized) */
	mach_msg_type_number_t		xmlDataLen;
	CFPropertyListRef		data;		/* data (un-serialized) */
	int				newInstance;
	int				sc_status;

	if (store == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;	/* you must have an open session to play */
	}

	/* serialize the key */
	if (!_SCSerializeString(key, &utfKey, (void **)&myKeyRef, &myKeyLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	/* send the key & fetch the associated data from the server */
	status = configget(storePrivate->server,
			   myKeyRef,
			   myKeyLen,
			   &xmlDataRef,
			   &xmlDataLen,
			   &newInstance,
			   (int *)&sc_status);

	/* clean up */
	CFRelease(utfKey);

	if (status != KERN_SUCCESS) {
		if (status == MACH_SEND_INVALID_DEST) {
			/* the server's gone and our session port's dead, remove the dead name right */
			(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
		} else {
			/* we got an unexpected error, leave the [session] port alone */
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreCopyValue configget(): %s"), mach_error_string(status));
		}
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		(void) vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the data */
	if (!_SCUnserialize(&data, NULL, xmlDataRef, xmlDataLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	return data;
}
