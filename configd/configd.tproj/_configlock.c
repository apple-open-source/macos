/*
 * Copyright (c) 2000, 2001, 2003, 2004, 2006, 2009 Apple Inc. All rights reserved.
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
#include "configd_server.h"
#include "session.h"


__private_extern__
int
__SCDynamicStoreLock(SCDynamicStoreRef store, Boolean recursive)
{
	serverSessionRef		mySession;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;

	if ((store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;		/* you must have an open session to play */
	}

	if (storeLocked > 0) {
		if (storePrivate->locked && recursive) {
			/* if this session holds the lock and this is a recursive (internal) request */
			storeLocked++;
			return kSCStatusOK;
		}
		return kSCStatusLocked;			/* sorry, someone (you) already have the lock */
	}

	/* check credentials */
	mySession = getSession(storePrivate->server);
	if (!hasWriteAccess(mySession)) {
		return kSCStatusAccessError;
	}

	if (!recursive && _configd_trace) {
		SCTrace(TRUE, _configd_trace, CFSTR("lock    : %5d\n"), storePrivate->server);
	}

	storeLocked          = 1;	/* global lock flag */
	storePrivate->locked = TRUE;	/* per-session lock flag */

	/*
	 * defer all (actually, most) changes until the call to __SCDynamicStoreUnlock()
	 */
	if (storeData_s) {
		CFRelease(storeData_s);
		CFRelease(patternData_s);
		CFRelease(changedKeys_s);
		CFRelease(deferredRemovals_s);
		CFRelease(removedSessionKeys_s);
	}
	storeData_s          = CFDictionaryCreateMutableCopy(NULL, 0, storeData);
	patternData_s        = CFDictionaryCreateMutableCopy(NULL, 0, patternData);
	changedKeys_s        = CFSetCreateMutableCopy(NULL, 0, changedKeys);
	deferredRemovals_s   = CFSetCreateMutableCopy(NULL, 0, deferredRemovals);
	removedSessionKeys_s = CFSetCreateMutableCopy(NULL, 0, removedSessionKeys);

	/* Add a "locked" mode run loop source for this port */
	CFRunLoopAddSource(CFRunLoopGetCurrent(), mySession->serverRunLoopSource, CFSTR("locked"));

	return kSCStatusOK;
}


__private_extern__
kern_return_t
_configlock(mach_port_t server, int *sc_status)
{
	serverSessionRef	mySession = getSession(server);

	if (mySession == NULL) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreLock(mySession->store, FALSE);
	if (*sc_status != kSCStatusOK) {
		return KERN_SUCCESS;
	}

	return KERN_SUCCESS;
}
