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

#include <SystemConfiguration/SCP.h>
#include "SCPPrivate.h"

#include <SystemConfiguration/SCD.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>


SCPStatus
SCPApply(SCPSessionRef session)
{
	SCPSessionPrivateRef	sessionPrivate;
	SCPStatus		scp_status = SCP_OK;
	SCDStatus		scd_status;
	boolean_t		wasLocked;

	if (session == NULL) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)session;

	/*
	 * Determine if the we have exclusive access to the preferences
	 * and acquire the lock if necessary.
	 */
	wasLocked = sessionPrivate->locked;
	if (!wasLocked) {
		scp_status = SCPLock(session, TRUE);
		if (scp_status != SCD_OK) {
			SCDLog(LOG_DEBUG, CFSTR("  SCPLock(): %s"), SCPError(scp_status));
			return scp_status;
		}
	}

	if (!sessionPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto notRoot;
	}

	/* if necessary, create the session "apply" key */
	if (sessionPrivate->sessionKeyApply == NULL) {
		sessionPrivate->sessionKeyApply = _SCPNotificationKey(sessionPrivate->prefsID,
								      sessionPrivate->perUser,
								      sessionPrivate->user,
								      kSCPKeyApply);
	}

	/* post notification */
	scd_status = SCDLock(sessionPrivate->session);
	if (scd_status == SCD_OK) {
		(void) SCDTouch (sessionPrivate->session, sessionPrivate->sessionKeyApply);
		(void) SCDRemove(sessionPrivate->session, sessionPrivate->sessionKeyApply);
		(void) SCDUnlock(sessionPrivate->session);
	} else {
		SCDLog(LOG_DEBUG, CFSTR("  SCDLock(): %s"), SCDError(scd_status));
		scp_status = SCP_FAILED;
	}

    notRoot:

	if (!wasLocked)
		(void) SCPUnlock(session);

	return scp_status;
}
