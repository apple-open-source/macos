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

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

Boolean
SCDynamicStoreAddWatchedKey(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			xmlKey;		/* serialized key */
	xmlData_t			myKeyRef;
	CFIndex				myKeyLen;
	int				sc_status;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreAddWatchedKey:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  key     = %@"), key);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  isRegex = %s"), isRegex ? "TRUE" : "FALSE");

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

	/*
	 * add new key after checking if key has already been defined
	 */
	if (isRegex) {
		if (CFSetContainsValue(storePrivate->reKeys, key)) {
			/* sorry, key already exists in notifier list */
			_SCErrorSet(kSCStatusKeyExists);
			return FALSE;
		}
		CFSetAddValue(storePrivate->reKeys, key);	/* add key to this sessions notifier list */
	} else {
		if (CFSetContainsValue(storePrivate->keys, key)) {
			/* sorry, key already exists in notifier list */
			_SCErrorSet(kSCStatusKeyExists);
			return FALSE;
		}
		CFSetAddValue(storePrivate->keys, key);	/* add key to this sessions notifier list */
	}

	/* serialize the key */
	if (!_SCSerialize(key, &xmlKey, (void **)&myKeyRef, &myKeyLen)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	/* send the key to the server */
	status = notifyadd(storePrivate->server,
			   myKeyRef,
			   myKeyLen,
			   isRegex,
			   (int *)&sc_status);

	/* clean up */
	CFRelease(xmlKey);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("notifyadd(): %s"), mach_error_string(status));
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
