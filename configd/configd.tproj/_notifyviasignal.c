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

int
__SCDynamicStoreNotifySignal(SCDynamicStoreRef store, pid_t pid, int sig)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	CFStringRef			sessionKey;
	CFDictionaryRef			info;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreNotifySignal:"));

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (storePrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return kSCStatusNotifierActive;
	}

	if (pid == getpid()) {
		/* sorry, you can't request that configd be signalled */
		return kSCStatusFailed;
	}

	if ((sig <= 0) || (sig > NSIG)) {
		/* sorry, you must specify a valid signal */
		return kSCStatusInvalidArgument;
	}

	/* push out a notification if any changes are pending */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);
	info = CFDictionaryGetValue(sessionData, sessionKey);
	CFRelease(sessionKey);
	if (info && CFDictionaryContainsKey(info, kSCDChangedKeys)) {
		CFNumberRef	sessionNum;

		if (needsNotification == NULL)
			needsNotification = CFSetCreateMutable(NULL,
							       0,
							       &kCFTypeSetCallBacks);

		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);
		CFSetAddValue(needsNotification, sessionNum);
		CFRelease(sessionNum);
	}

	return kSCStatusOK;
}


kern_return_t
_notifyviasignal(mach_port_t	server,
		 task_t		task,
		 int		sig,
		 int		*sc_status)
{
	serverSessionRef		mySession  = getSession(server);
	pid_t				pid;
	kern_return_t			status;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)mySession->store;
#ifdef	NOTYET
	mach_port_t			oldNotify;
#endif	/* NOTYET */

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Send signal when a notification key changes."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  task   = %d"), task);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  signal = %d"), sig);

	status = pid_for_task(task, &pid);
	if (status != KERN_SUCCESS) {
		/* could not determine pid for task */
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreNotifySignal(mySession->store, pid, sig);
	if (*sc_status != kSCStatusOK) {
		if (task != TASK_NULL) {
			(void) mach_port_destroy(mach_task_self(), task);
		}
		return KERN_SUCCESS;
	}

#ifdef	DEBUG
	{	mach_port_type_t	pt;

		status = mach_port_type(mach_task_self(), task, &pt);
		if (status == MACH_MSG_SUCCESS) {
			char	rights[8], *rp = &rights[0];

			if (pt & MACH_PORT_TYPE_SEND)
				*rp++ = 'S';
			if (pt & MACH_PORT_TYPE_RECEIVE)
				*rp++ = 'R';
			if (pt & MACH_PORT_TYPE_SEND_ONCE)
				*rp++ = 'O';
			if (pt & MACH_PORT_TYPE_PORT_SET)
				*rp++ = 'P';
			if (pt & MACH_PORT_TYPE_DEAD_NAME)
				*rp++ = 'D';
			*rp = '\0';

			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Task %d, port rights = %s"), task, rights);
		}
	}
#endif	/* DEBUG */

#ifdef	NOTYET
	/* Request a notification when/if the client dies */
	status = mach_port_request_notification(mach_task_self(),
						task,
						MACH_NOTIFY_DEAD_NAME,
						1,
						task,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&oldNotify);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	if (oldNotify != MACH_PORT_NULL) {
		SCLog(_configd_verbose, LOG_ERR, CFSTR("_notifyviasignal(): why is oldNotify != MACH_PORT_NULL?"));
	}

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Adding task notification port %d to the server's port set"), task);
	status = mach_port_move_member(mach_task_self(), task, server_ports);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("mach_port_move_member(): %s"), mach_error_string(status));
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}
#endif	/* NOTYET */

	storePrivate->notifyStatus     = Using_NotifierInformViaSignal;
	storePrivate->notifySignal     = sig;
	storePrivate->notifySignalTask = task;

	return KERN_SUCCESS;
}
