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

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

CFArrayRef
SCDynamicStoreCopyKeyList(SCDynamicStoreRef store, CFStringRef pattern)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			utfPattern;	/* serialized pattern */
	xmlData_t			myPatternRef;
	CFIndex				myPatternLen;
	xmlDataOut_t			xmlDataRef;	/* serialized data */
	int				xmlDataLen;
	int				sc_status;
	CFArrayRef			allKeys;

	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreCopyKeyList:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  pattern = %@"), pattern);
	}

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;
	}

	/* serialize the pattern */
	if (!_SCSerializeString(pattern, &utfPattern, (void **)&myPatternRef, &myPatternLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	/* send the pattern & fetch the associated data from the server */
	status = configlist(storePrivate->server,
			    myPatternRef,
			    myPatternLen,
			    TRUE,		/* isRegex == TRUE */
			    &xmlDataRef,
			    &xmlDataLen,
			    (int *)&sc_status);

	/* clean up */
	CFRelease(utfPattern);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configlist(): %s"), mach_error_string(status));
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

	/* un-serialize the list of keys */
	if (!_SCUnserialize((CFPropertyListRef *)&allKeys, NULL, xmlDataRef, xmlDataLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	return allKeys;
}
