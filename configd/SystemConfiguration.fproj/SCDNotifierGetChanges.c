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

CFArrayRef
SCDynamicStoreCopyNotifiedKeys(SCDynamicStoreRef store)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	xmlDataOut_t			xmlDataRef;	/* serialized data */
	int				xmlDataLen;
	int				sc_status;
	CFArrayRef			allKeys;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreCopyNotifiedKeys:"));

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;
	}

	/* send the key & fetch the associated data from the server */
	status = notifychanges(storePrivate->server,
			       &xmlDataRef,
			       &xmlDataLen,
			       (int *)&sc_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("notifychanges(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the list of keys which have changed */
	if (!_SCUnserialize((CFPropertyListRef *)&allKeys, xmlDataRef, xmlDataLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	return allKeys;
}
