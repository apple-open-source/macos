/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


Boolean
SCDynamicStoreSetMultiple(SCDynamicStoreRef	store,
			  CFDictionaryRef	keysToSet,
			  CFArrayRef		keysToRemove,
			  CFArrayRef		keysToNotify)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			xmlSet		= NULL;	/* key/value pairs to set (XML serialized) */
	xmlData_t			mySetRef	= NULL;	/* key/value pairs to set (serialized) */
	CFIndex				mySetLen	= 0;
	CFDataRef			xmlRemove	= NULL;	/* keys to remove (XML serialized) */
	xmlData_t			myRemoveRef	= NULL;	/* keys to remove (serialized) */
	CFIndex				myRemoveLen	= 0;
	CFDataRef			xmlNotify	= NULL;	/* keys to notify (XML serialized) */
	xmlData_t			myNotifyRef	= NULL;	/* keys to notify (serialized) */
	CFIndex				myNotifyLen	= 0;
	int				sc_status;

	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetMultiple:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keysToSet    = %@"), keysToSet);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keysToRemove = %@"), keysToRemove);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keysToNotify = %@"), keysToNotify);
	}

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;	/* you must have an open session to play */
	}

	/* serialize the key/value pairs to set*/
	if (keysToSet) {
		CFDictionaryRef	newInfo;
		Boolean		ok;

		newInfo = _SCSerializeMultiple(keysToSet);
		if (!newInfo) {
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}

		ok = _SCSerialize(newInfo, &xmlSet, (void **)&mySetRef, &mySetLen);
		CFRelease(newInfo);

		if (!ok) {
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	/* serialize the keys to remove */
	if (keysToRemove) {
		if (!_SCSerialize(keysToRemove, &xmlRemove, (void **)&myRemoveRef, &myRemoveLen)) {
			if (xmlSet)	CFRelease(xmlSet);
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	/* serialize the keys to notify */
	if (keysToNotify) {
		if (!_SCSerialize(keysToNotify, &xmlNotify, (void **)&myNotifyRef, &myNotifyLen)) {
			if (xmlSet)	CFRelease(xmlSet);
			if (xmlRemove)	CFRelease(xmlRemove);
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	/* send the keys and patterns, fetch the associated result from the server */
	status = configset_m(storePrivate->server,
			     mySetRef,
			     mySetLen,
			     myRemoveRef,
			     myRemoveLen,
			     myNotifyRef,
			     myNotifyLen,
			     (int *)&sc_status);

	/* clean up */
	if (xmlSet)	CFRelease(xmlSet);
	if (xmlRemove)	CFRelease(xmlRemove);
	if (xmlNotify)	CFRelease(xmlNotify);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configset_m(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return FALSE;
	}

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	return TRUE;
}

Boolean
SCDynamicStoreSetValue(SCDynamicStoreRef store, CFStringRef key, CFPropertyListRef value)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			utfKey;		/* serialized key */
	xmlData_t			myKeyRef;
	CFIndex				myKeyLen;
	CFDataRef			xmlData;	/* serialized data */
	xmlData_t			myDataRef;
	CFIndex				myDataLen;
	int				sc_status;
	int				newInstance;

	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetValue:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  key          = %@"), key);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  value        = %@"), value);
	}

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		/* sorry, you must have an open session to play */
		_SCErrorSet(kSCStatusNoStoreServer);
		return FALSE;
	}

	/* serialize the key */
	if (!_SCSerializeString(key, &utfKey, (void **)&myKeyRef, &myKeyLen)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	/* serialize the data */
	if (!_SCSerialize(value, &xmlData, (void **)&myDataRef, &myDataLen)) {
		CFRelease(utfKey);
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	/* send the key & data to the server, get new instance id */
	status = configset(storePrivate->server,
			   myKeyRef,
			   myKeyLen,
			   myDataRef,
			   myDataLen,
			   0,
			   &newInstance,
			   (int *)&sc_status);

	/* clean up */
	CFRelease(utfKey);
	CFRelease(xmlData);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configset(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return FALSE;
	}

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	return TRUE;
}
