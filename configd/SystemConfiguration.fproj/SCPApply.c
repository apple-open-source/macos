/*
 * Copyright(c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1(the
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
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCPreferencesInternal.h"

Boolean
SCPreferencesApplyChanges(SCPreferencesRef session)
{
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;
	Boolean			wasLocked;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesApplyChanges:"));

	/*
	 * Determine if the we have exclusive access to the preferences
	 * and acquire the lock if necessary.
	 */
	wasLocked = sessionPrivate->locked;
	if (!wasLocked) {
		if (!SCPreferencesLock(session, TRUE)) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  SCPreferencesLock() failed"));
			return FALSE;
		}
	}

	if (!sessionPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto perUser;
	}

	/* if necessary, create the session "apply" key */
	if (sessionPrivate->sessionKeyApply == NULL) {
		sessionPrivate->sessionKeyApply = _SCPNotificationKey(NULL,
								      sessionPrivate->prefsID,
								      sessionPrivate->perUser,
								      sessionPrivate->user,
								      kSCPreferencesKeyApply);
	}

	/* post notification */
	if (!SCDynamicStoreNotifyValue(sessionPrivate->session,
				       sessionPrivate->sessionKeyApply)) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  SCDynamicStoreNotifyValue() failed"));
		_SCErrorSet(kSCStatusFailed);
		goto error;
	}

    perUser :

	if (!wasLocked)	(void) SCPreferencesUnlock(session);
	return TRUE;

    error :

	if (!wasLocked)	(void) SCPreferencesUnlock(session);
	return FALSE;
}
