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
SCDNotifierCancel(SCDSessionRef session)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	SCDStatus		scd_status;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierCancel:"));

	if (session == NULL) {
		return SCD_NOSESSION;		/* you can't do anything without a session */
	}

	if (sessionPrivate->notifyStatus == NotifierNotRegistered) {
		/* nothing to do, no notifications have been registered */
		return SCD_OK;
	}

	/*  if SCDNotifierInformViaCallback() active, stop the background thread  */
	if (sessionPrivate->callbackFunction != NULL) {

		if (SCDOptionGet(session, kSCDOptionUseCFRunLoop)) {
			SCDLog(LOG_DEBUG, CFSTR("  cancel callback runloop source"));

			/* XXX invalidating the port is not sufficient, remove the run loop source */
			CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
					      sessionPrivate->callbackRunLoopSource,
					      kCFRunLoopDefaultMode);
			CFRelease(sessionPrivate->callbackRunLoopSource);

			/* invalidate port */
			CFMachPortInvalidate(sessionPrivate->callbackPort);
			CFRelease(sessionPrivate->callbackPort);
		} else {
			int		ts;

			SCDLog(LOG_DEBUG, CFSTR("  cancel callback thread"));
			ts = pthread_cancel(sessionPrivate->callbackHelper);
			if (ts != 0) {
				SCDLog(LOG_DEBUG, CFSTR("  pthread_cancel(): %s"), strerror(ts));
			}
		}

		sessionPrivate->callbackFunction	= NULL;
		sessionPrivate->callbackArgument	= NULL;
		sessionPrivate->callbackPort		= NULL;
		sessionPrivate->callbackRunLoopSource	= NULL;	/* XXX */
		sessionPrivate->callbackHelper		= NULL;
	}

	if (sessionPrivate->server == MACH_PORT_NULL) {
		return SCD_NOSESSION;		/* you must have an open session to play */
	}

	status = notifycancel(sessionPrivate->server, (int *)&scd_status);

	/* set notifier inactive */
	sessionPrivate->notifyStatus = NotifierNotRegistered;

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifycancel(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	return scd_status;
}
