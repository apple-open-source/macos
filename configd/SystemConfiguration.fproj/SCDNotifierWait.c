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
SCDNotifierWait(SCDSessionRef session)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	mach_port_t		port;
	mach_port_t		oldNotify;
	SCDStatus		scd_status;
	mach_msg_id_t		msgid;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierWait:"));

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

	SCDLog(LOG_DEBUG, CFSTR("Allocating port (for server response)"));
	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("mach_port_allocate(): %s"), mach_error_string(status));
		return SCD_FAILED;
	}
	SCDLog(LOG_DEBUG, CFSTR("  port = %d"), port);

	status = mach_port_insert_right(mach_task_self(),
					port,
					port,
					MACH_MSG_TYPE_MAKE_SEND);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("mach_port_insert_right(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), port);
		return SCD_FAILED;
	}

	/* Request a notification when/if the server dies */
	status = mach_port_request_notification(mach_task_self(),
						port,
						MACH_NOTIFY_NO_SENDERS,
						1,
						port,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&oldNotify);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), port);
		return SCD_FAILED;
	}

#ifdef	DEBUG
	if (oldNotify != MACH_PORT_NULL) {
		SCDLog(LOG_DEBUG, CFSTR("SCDNotifierWait(): why is oldNotify != MACH_PORT_NULL?"));
	}
#endif	/* DEBUG */

	SCDLog(LOG_DEBUG, CFSTR("Requesting notification via mach port %d"), port);
	status = notifyviaport(sessionPrivate->server,
			       port,
			       0,
			       (int *)&scd_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifyviaport(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	if (scd_status != SCD_OK) {
		return scd_status;
	}

	/* set notifier active */
	sessionPrivate->notifyStatus = Using_NotifierWait;

	SCDLog(LOG_DEBUG, CFSTR("Waiting..."));

	msgid = _waitForMachMessage(port);

	/* set notifier inactive */
	sessionPrivate->notifyStatus = NotifierNotRegistered;

	if (msgid == MACH_NOTIFY_NO_SENDERS) {
		/* the server closed the notifier port */
		SCDLog(LOG_DEBUG, CFSTR("  notifier port closed, destroying port %d"), port);
		return SCD_NOSERVER;
	}

	if (msgid == -1) {
		/* one of the mach routines returned an error */
		SCDLog(LOG_DEBUG, CFSTR("  communication with server failed, destroying port %d"), port);
		(void) mach_port_destroy(mach_task_self(), port);
		return SCD_NOSERVER;
	}

	SCDLog(LOG_DEBUG, CFSTR("Something changed, cancelling notification request"));
	status = notifycancel(sessionPrivate->server,
			      (int *)&scd_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifycancel(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		scd_status = SCD_NOSERVER;
	}

	(void) mach_port_destroy(mach_task_self(), port);

	return scd_status;
}
