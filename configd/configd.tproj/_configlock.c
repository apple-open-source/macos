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
#include "configd_server.h"
#include "session.h"


SCDStatus
_SCDLock(SCDSessionRef session)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	serverSessionRef	mySession;

	SCDLog(LOG_DEBUG, CFSTR("_SCDLock:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	if (SCDOptionGet(NULL, kSCDOptionIsLocked)) {
		return SCD_LOCKED;	/* sorry, someone (you) already have the lock */
	}

	/* check credentials */
	mySession = getSession(sessionPrivate->server);
	if (mySession->callerEUID != 0) {
#ifdef	DEBUG
		if (!SCDOptionGet(NULL, kSCDOptionDebug)) {
#endif	/* DEBUG */
			return SCD_EACCESS;
#ifdef	DEBUG
		} else {
			SCDLog(LOG_DEBUG, CFSTR("  non-root access granted while debugging"));
		}
#endif	/* DEBUG */
	}

	SCDOptionSet(NULL,    kSCDOptionIsLocked, TRUE);	/* global lock flag */
	SCDOptionSet(session, kSCDOptionIsLocked, TRUE);	/* per-session lock flag */

	/*
	 * defer all (actually, most) changes until the call to _SCDUnlock()
	 */
	if (cacheData_s) {
		CFRelease(cacheData_s);
		CFRelease(changedKeys_s);
		CFRelease(deferredRemovals_s);
		CFRelease(removedSessionKeys_s);
	}
	cacheData_s          = CFDictionaryCreateMutableCopy(NULL, 0, cacheData);
	changedKeys_s        = CFSetCreateMutableCopy(NULL, 0, changedKeys);
	deferredRemovals_s   = CFSetCreateMutableCopy(NULL, 0, deferredRemovals);
	removedSessionKeys_s = CFSetCreateMutableCopy(NULL, 0, removedSessionKeys);

	/* Add a "locked" mode run loop source for this port */
	CFRunLoopAddSource(CFRunLoopGetCurrent(), mySession->serverRunLoopSource, CFSTR("locked"));

	return SCD_OK;
}


kern_return_t
_configlock(mach_port_t server, int *scd_status)
{
	serverSessionRef	mySession = getSession(server);

	SCDLog(LOG_DEBUG, CFSTR("Lock configuration database."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	*scd_status = _SCDLock(mySession->session);
	if (*scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("  SCDLock(): %s"), SCDError(*scd_status));
		return KERN_SUCCESS;
	}

	return KERN_SUCCESS;
}
