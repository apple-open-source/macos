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

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


SCDStatus
SCDNotifierInformViaSignal(SCDSessionRef session, pid_t pid, int sig)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;

	kern_return_t	status;
	SCDStatus	scd_status;
	task_t		task;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaSignal:"));
	SCDLog(LOG_DEBUG, CFSTR("  pid = %d"), pid);
	SCDLog(LOG_DEBUG, CFSTR("  sig = %d"), sig);

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	if (SCDOptionGet(NULL, kSCDOptionIsServer)) {
		/* sorry, neither the server nor any plug-ins can "wait" */
		return SCD_FAILED;
	}

	if (sessionPrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return SCD_NOTIFIERACTIVE;
	}

	if ((sig <= 0) || (sig > NSIG)) {
		/* sorry, you must specify a valid signal */
		return SCD_INVALIDARGUMENT;
	}

	status = task_for_pid(mach_task_self(), pid, &task);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("task_for_pid(): %s"), mach_error_string(status));
		return SCD_FAILED;
	}

	status = notifyviasignal(sessionPrivate->server, task, sig, (int *)&scd_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifyviasignal(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	/* set notifier active */
	sessionPrivate->notifyStatus = Using_NotifierInformViaSignal;

	return scd_status;
}
