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
SCPClose(SCPSessionRef *session)
{
	SCPSessionPrivateRef	sessionPrivate;

	if ((session == NULL) || (*session == NULL)) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)*session;

	/* release resources */
	if (sessionPrivate->name)		CFRelease(sessionPrivate->name);
	if (sessionPrivate->prefsID)		CFRelease(sessionPrivate->prefsID);
	if (sessionPrivate->user)		CFRelease(sessionPrivate->user);
	if (sessionPrivate->path)		CFAllocatorDeallocate(NULL, sessionPrivate->path);
	if (sessionPrivate->signature)		CFRelease(sessionPrivate->signature);
	if (sessionPrivate->session) {
		SCDStatus	scd_status;

		scd_status = SCDClose(&sessionPrivate->session);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_INFO, CFSTR("SCDClose() failed: %s"), SCDError(scd_status));
		}
	}
	if (sessionPrivate->sessionKeyLock)	CFRelease(sessionPrivate->sessionKeyLock);
	if (sessionPrivate->sessionKeyCommit)	CFRelease(sessionPrivate->sessionKeyCommit);
	if (sessionPrivate->sessionKeyApply)	CFRelease(sessionPrivate->sessionKeyApply);
	if (sessionPrivate->prefs)		CFRelease(sessionPrivate->prefs);

	/* release session */
	CFAllocatorDeallocate(NULL, sessionPrivate);
	*session = NULL;
	return SCP_OK;
}
