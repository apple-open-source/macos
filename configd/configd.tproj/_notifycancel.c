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

#include <unistd.h>

#include "configd.h"
#include "session.h"


SCDStatus
_SCDNotifierCancel(SCDSessionRef session)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;

	SCDLog(LOG_DEBUG, CFSTR("_SCDNotifierCancel:"));

	if (session == NULL) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	/*
	 * cleanup any mach port based notifications.
	 */
	if (sessionPrivate->notifyPort != MACH_PORT_NULL) {
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->notifyPort);
		sessionPrivate->notifyPort = MACH_PORT_NULL;
	}

	/*
	 * cleanup any file based notifications.
	 */
	if (sessionPrivate->notifyFile >= 0) {
		SCDLog(LOG_DEBUG, CFSTR("  closing (notification) fd %d"), sessionPrivate->notifyFile);
		(void) close(sessionPrivate->notifyFile);
		sessionPrivate->notifyFile = -1;
	}

	/*
	 * cleanup any signal notifications.
	 */
	if (sessionPrivate->notifySignal > 0) {
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->notifySignalTask);
		sessionPrivate->notifySignal     = 0;
		sessionPrivate->notifySignalTask = TASK_NULL;
	}

	/* remove this session from the to-be-notified list */
	if (needsNotification) {
		CFNumberRef	num;

		num = CFNumberCreate(NULL, kCFNumberIntType, &sessionPrivate->server);
		CFSetRemoveValue(needsNotification, num);
		CFRelease(num);

		if (CFSetGetCount(needsNotification) == 0) {
			CFRelease(needsNotification);
			needsNotification = NULL;
		}
	}

	/* set notifier inactive */
	sessionPrivate->notifyStatus = NotifierNotRegistered;

	return SCD_OK;
}


kern_return_t
_notifycancel(mach_port_t	server,
	      int		*scd_status)
{
	serverSessionRef	mySession = getSession(server);

	SCDLog(LOG_DEBUG, CFSTR("Cancel requested notifications."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	*scd_status = _SCDNotifierCancel(mySession->session);
	if (*scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("  SCDNotifierCancel(): %s"), SCDError(*scd_status));
	}

	return KERN_SUCCESS;
}
