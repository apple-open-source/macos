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
SCPUnlock(SCPSessionRef session)
{
	SCPSessionPrivateRef	sessionPrivate;
	SCDStatus		scd_status;

	if (session == NULL) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)session;

	if (!sessionPrivate->locked) {
		return SCP_NEEDLOCK;	/* sorry, you don't have the lock */
	}

	if (!sessionPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto notRoot;
	}

	scd_status = SCDRemove(sessionPrivate->session, sessionPrivate->sessionKeyLock);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDRemove() failed: %s"), SCDError(scd_status));
		return SCP_FAILED;
	}

    notRoot:

	sessionPrivate->locked = FALSE;
	return SCP_OK;
}
