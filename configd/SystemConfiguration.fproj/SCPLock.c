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
SCPLock(SCPSessionRef session, boolean_t wait)
{
	SCPStatus		scp_status;
	SCDStatus		scd_status;
	SCPSessionPrivateRef	sessionPrivate;
	SCDHandleRef		handle = NULL;
	CFDateRef		value;
	CFArrayRef		changes;
	struct stat		statBuf;
	CFDataRef		currentSignature;

	if (session == NULL) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)session;

	if (!sessionPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto notRoot;
	}

	if (sessionPrivate->session == NULL) {
		/* open a session */
		scd_status = SCDOpen(&sessionPrivate->session, sessionPrivate->name);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_INFO, CFSTR("SCDOpen() failed: %s"), SCDError(scd_status));
			return SCP_FAILED;
		}
	}

	if (sessionPrivate->sessionKeyLock == NULL) {
		/* create the session "lock" key */
		sessionPrivate->sessionKeyLock = _SCPNotificationKey(sessionPrivate->prefsID,
								     sessionPrivate->perUser,
								     sessionPrivate->user,
								     kSCPKeyLock);
	}

	scd_status = SCDNotifierAdd(sessionPrivate->session,
				    sessionPrivate->sessionKeyLock,
				    0);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDNotifierAdd() failed: %s"), SCDError(scd_status));
		scp_status = SCP_FAILED;
		goto error;
	}

	handle = SCDHandleInit();
	value  = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	SCDHandleSetData(handle, value);
	CFRelease(value);

	while (TRUE) {
		/*
		 * Attempt to acquire the lock
		 */
		scd_status = SCDAddSession(sessionPrivate->session,
					   sessionPrivate->sessionKeyLock,
					   handle);
		switch (scd_status) {
			case SCD_OK :
				scp_status = SCP_OK;
				goto done;
			case SCD_EXISTS :
				if (!wait) {
					scp_status = SCP_BUSY;
					goto error;
				}
				break;
			default :
				SCDLog(LOG_INFO, CFSTR("SCDAddSession() failed: %s"), SCDError(scd_status));
				scp_status = SCP_FAILED;
				goto error;
		}

		/*
		 * Wait for the lock to be released
		 */
		scd_status = SCDNotifierWait(sessionPrivate->session);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_INFO, CFSTR("SCDAddSession() failed: %s"), SCDError(scd_status));
			scp_status = SCP_FAILED;
			goto error;
		}
	}

    done :

	SCDHandleRelease(handle);
	handle = NULL;

	scd_status = SCDNotifierRemove(sessionPrivate->session,
				       sessionPrivate->sessionKeyLock,
				       0);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDNotifierRemove() failed: %s"), SCDError(scd_status));
		scp_status = SCP_FAILED;
		goto error;
	}

	scd_status = SCDNotifierGetChanges(sessionPrivate->session, &changes);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDNotifierGetChanges() failed: %s"), SCDError(scd_status));
		scp_status = SCP_FAILED;
		goto error;
	}
	CFRelease(changes);

    notRoot:

	/*
	 * Check the signature
	 */
	if (stat(sessionPrivate->path, &statBuf) == -1) {
		if (errno == ENOENT) {
			bzero(&statBuf, sizeof(statBuf));
		} else {
			SCDLog(LOG_DEBUG, CFSTR("stat() failed: %s"), strerror(errno));
			scp_status = SCP_STALE;
			goto error;
		}
	}

	currentSignature = _SCPSignatureFromStatbuf(&statBuf);
	if (!CFEqual(sessionPrivate->signature, currentSignature)) {
		CFRelease(currentSignature);
		scp_status = SCP_STALE;
		goto error;
	}
	CFRelease(currentSignature);

	sessionPrivate->locked = TRUE;
	return SCD_OK;

    error :

	if (handle)	SCDHandleRelease(handle);
	return scp_status;
}
