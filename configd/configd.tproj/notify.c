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
#include "configd_server.h"
#include "session.h"


void
pushNotifications()
{
	void			**sessionsToNotify;
	CFIndex			notifyCnt;
	int			server;
	serverSessionRef	theSession;
	SCDSessionPrivateRef	sessionPrivate;

	if (needsNotification == NULL)
		return;		/* if no sessions need to be kicked */

	notifyCnt = CFSetGetCount(needsNotification);
	sessionsToNotify = malloc(notifyCnt * sizeof(CFNumberRef));
	CFSetGetValues(needsNotification, sessionsToNotify);
	while (--notifyCnt >= 0) {
		(void) CFNumberGetValue(sessionsToNotify[notifyCnt],
					kCFNumberIntType,
					&server);
		theSession = getSession(server);
		sessionPrivate = (SCDSessionPrivateRef)theSession->session;

		/*
		 * handle callbacks for "configd" plug-ins
		 */
		if (sessionPrivate->callbackFunction != NULL) {
			SCDLog(LOG_DEBUG, CFSTR("executing notifiction callback function (server=%d)."),
			       sessionPrivate->server);
			(void) (*sessionPrivate->callbackFunction)(theSession->session,
								   sessionPrivate->callbackArgument);
		}

		/*
		 * deliver notifications to client sessions
		 */
		if (sessionPrivate->notifyPort != MACH_PORT_NULL) {
			mach_msg_empty_send_t	msg;
			mach_msg_option_t	options;
			kern_return_t		status;
			/*
			 * Post notification as mach message
			 */
			SCDLog(LOG_DEBUG, CFSTR("sending mach message notification."));
			SCDLog(LOG_DEBUG, CFSTR("  port  = %d"), sessionPrivate->notifyPort);
			SCDLog(LOG_DEBUG, CFSTR("  msgid = %d"), sessionPrivate->notifyPortIdentifier);
			msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
			msg.header.msgh_size = sizeof(msg);
			msg.header.msgh_remote_port = sessionPrivate->notifyPort;
			msg.header.msgh_local_port = MACH_PORT_NULL;
			msg.header.msgh_id = sessionPrivate->notifyPortIdentifier;
			options = MACH_SEND_TIMEOUT;
			status = mach_msg(&msg.header,			/* msg */
					  MACH_SEND_MSG|options,	/* options */
					  msg.header.msgh_size,		/* send_size */
					  0,				/* rcv_size */
					  MACH_PORT_NULL,		/* rcv_name */
					  0,				/* timeout */
					  MACH_PORT_NULL);		/* notify */
		}

		if (sessionPrivate->notifyFile >= 0) {
			ssize_t		written;

			SCDLog(LOG_DEBUG, CFSTR("sending (UNIX domain) socket notification"));
			SCDLog(LOG_DEBUG, CFSTR("  fd    = %d"), sessionPrivate->notifyFile);
			SCDLog(LOG_DEBUG, CFSTR("  msgid = %d"), sessionPrivate->notifyFileIdentifier);

			written = write(sessionPrivate->notifyFile,
					&sessionPrivate->notifyFileIdentifier,
					sizeof(sessionPrivate->notifyFileIdentifier));
			if (written == -1) {
				if (errno == EWOULDBLOCK) {
					SCDLog(LOG_DEBUG,
					       CFSTR("sorry, only one outstanding notification per session."));
				} else {
					SCDLog(LOG_DEBUG,
					       CFSTR("could not send notification, write() failed: %s"),
					       strerror(errno));
					sessionPrivate->notifyFile = -1;
				}
			} else if (written != sizeof(sessionPrivate->notifyFileIdentifier)) {
				SCDLog(LOG_DEBUG,
				       CFSTR("could not send notification, incomplete write()"));
				sessionPrivate->notifyFile = -1;
			}
		}

		if (sessionPrivate->notifySignal > 0) {
			kern_return_t	status;
			pid_t		pid;
			/*
			 * Post notification as signal
			 */
			status = pid_for_task(sessionPrivate->notifySignalTask, &pid);
			if (status == KERN_SUCCESS) {
				SCDLog(LOG_DEBUG, CFSTR("sending signal notification"));
				SCDLog(LOG_DEBUG, CFSTR("  pid    = %d"), pid);
				SCDLog(LOG_DEBUG, CFSTR("  signal = %d"), sessionPrivate->notifySignal);
				if (kill(pid, sessionPrivate->notifySignal) != 0) {
					SCDLog(LOG_DEBUG, CFSTR("could not send signal: %s"), strerror(errno));
					status = KERN_FAILURE;
				}
			} else {
				mach_port_type_t	pt;

				if ((mach_port_type(mach_task_self(), sessionPrivate->notifySignalTask, &pt) == KERN_SUCCESS) &&
				    (pt & MACH_PORT_TYPE_DEAD_NAME)) {
					SCDLog(LOG_DEBUG, CFSTR("could not send signal, process died"));
				} else {
					SCDLog(LOG_DEBUG, CFSTR("could not send signal: %s"), mach_error_string(status));
				}
			}

			if (status != KERN_SUCCESS) {
				/* don't bother with any more attempts */
				(void) mach_port_destroy(mach_task_self(), sessionPrivate->notifySignalTask);
				sessionPrivate->notifySignal     = 0;
				sessionPrivate->notifySignalTask = TASK_NULL;
			}
	       }
	}
	free(sessionsToNotify);

	/*
	 * this list of notifications have been posted, wait for some more.
	 */
	CFRelease(needsNotification);
	needsNotification = NULL;

	return;
}
