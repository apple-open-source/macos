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


static void
informCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	SCDSessionRef		session        = (SCDSessionRef)info;
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	mach_msg_empty_rcv_t	*buf           = msg;
	mach_msg_id_t		msgid          = buf->header.msgh_id;
	SCDCallbackRoutine_t	cbFunc         = sessionPrivate->callbackFunction;
	void			*cbArg         = sessionPrivate->callbackArgument;

	if (msgid == MACH_NOTIFY_NO_SENDERS) {
		/* the server died, disable additional callbacks */
		SCDLog(LOG_DEBUG, CFSTR("  notifier port closed, disabling notifier"));
	} else if (cbFunc == NULL) {
		/* there is no (longer) a callback function, disable additional callbacks */
		SCDLog(LOG_DEBUG, CFSTR("  no callback function, disabling notifier"));
	} else {
		SCDLog(LOG_DEBUG, CFSTR("  executing notifiction function"));
		if ((*cbFunc)(session, cbArg)) {
			/*
			 * callback function returned success.
			 */
			return;
		} else {
			SCDLog(LOG_DEBUG, CFSTR("  callback returned error, disabling notifier"));
		}
	}

#ifdef	DEBUG
	if (port != sessionPrivate->callbackPort) {
		SCDLog(LOG_DEBUG, CFSTR("informCallback, why is port != callbackPort?"));
	}
#endif	/* DEBUG */

	/* we have encountered some type of error, disable additional callbacks */

	/* XXX invalidating the port is not sufficient, remove the run loop source */
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
			      sessionPrivate->callbackRunLoopSource,
			      kCFRunLoopDefaultMode);
	CFRelease(sessionPrivate->callbackRunLoopSource);

	/* invalidate port */
	CFMachPortInvalidate(port);
	CFRelease(port);

	sessionPrivate->notifyStatus     	= NotifierNotRegistered;
	sessionPrivate->callbackFunction 	= NULL;
	sessionPrivate->callbackArgument 	= NULL;
	sessionPrivate->callbackPort     	= NULL;
	sessionPrivate->callbackRunLoopSource	= NULL;	/* XXX */

	return;
}


static void
cleanupMachPort(void *ptr)
{
	mach_port_t	*port = (mach_port_t *)ptr;

	SCDLog(LOG_DEBUG, CFSTR("  cleaning up notification port %d"), *port);
	if (*port != MACH_PORT_NULL) {
		(void) mach_port_destroy(mach_task_self(), *port);
		free(port);
	}

	return;
}


static void *
watcherThread(void *arg)
{
	SCDSessionRef		session        = (SCDSessionRef)arg;
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	SCDCallbackRoutine_t	cbFunc         = sessionPrivate->callbackFunction;
	void			*cbArg         = sessionPrivate->callbackArgument;
	mach_port_t		*port          = malloc(sizeof(mach_port_t));

	*port = CFMachPortGetPort(sessionPrivate->callbackPort);
	pthread_cleanup_push(cleanupMachPort, (void *)port);

	while (TRUE) {
		mach_msg_id_t	msgid;

		SCDLog(LOG_DEBUG, CFSTR("Callback thread waiting, port=%d, tid=0x%08x"),
		       *port, pthread_self());

		msgid = _waitForMachMessage(*port);

		if (msgid == MACH_NOTIFY_NO_SENDERS) {
			/* the server closed the notifier port, disable additional callbacks */
			SCDLog(LOG_DEBUG, CFSTR("  notifier port closed, disabling notifier"));
			break;
		}

		if (msgid == -1) {
			mach_port_type_t	pt;

			/* an error was detected, disable additional callbacks */
			SCDLog(LOG_DEBUG, CFSTR("  server failure, disabling notifier"));

			/* check if the server connection is not valid, close if necessary */
			if ((mach_port_type(mach_task_self(), sessionPrivate->server, &pt) == KERN_SUCCESS) &&
			    (pt & MACH_PORT_TYPE_DEAD_NAME)) {
				SCDLog(LOG_DEBUG, CFSTR("  server process died, destroying (dead) port"));
				(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
				sessionPrivate->server = MACH_PORT_NULL;
			}
			break;
		}

		if (cbFunc == NULL) {
			/* there is no (longer) a callback function, disable additional callbacks */
			SCDLog(LOG_DEBUG, CFSTR("  no callback function, disabling notifier"));
			break;
		}

		SCDLog(LOG_DEBUG, CFSTR("  executing notifiction function"));

		if (!(*cbFunc)(session, cbArg)) {
			/*
			 * callback function returned an error, exit the thread
			 */
			break;
		}

	}

	/*
	 * pop the cleanup routine for the "port" mach port. We end up calling
	 * mach_port_destroy() in the process.
	 */
	pthread_cleanup_pop(1);

	pthread_exit (NULL);
	return NULL;
}


SCDStatus
SCDNotifierInformViaCallback(SCDSessionRef session, SCDCallbackRoutine_t func, void *arg)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	mach_port_t		port;
	mach_port_t		oldNotify;
	SCDStatus		scd_status;
	CFMachPortContext	context = { 0, (void *)session, NULL, NULL, NULL };

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaCallback:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	if (sessionPrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return SCD_NOTIFIERACTIVE;
	}

	if (func == NULL) {
		/* sorry, you must specify a callback function */
		return SCD_INVALIDARGUMENT;
	}

	/* Allocating port (for server response) */
	sessionPrivate->callbackPort = CFMachPortCreate(NULL,
							informCallback,
							&context,
							NULL);

	/* Request a notification when/if the server dies */
	port = CFMachPortGetPort(sessionPrivate->callbackPort);
	status = mach_port_request_notification(mach_task_self(),
						port,
						MACH_NOTIFY_NO_SENDERS,
						1,
						port,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&oldNotify);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
		CFMachPortInvalidate(sessionPrivate->callbackPort);
		CFRelease(sessionPrivate->callbackPort);
		return SCD_FAILED;
	}

#ifdef	DEBUG
	if (oldNotify != MACH_PORT_NULL) {
		SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaCallback(): why is oldNotify != MACH_PORT_NULL?"));
	}
#endif	/* DEBUG */

	/* Requesting notification via mach port */
	status = notifyviaport(sessionPrivate->server,
			       port,
			       0,
			       (int *)&scd_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifyviaport(): %s"), mach_error_string(status));
		CFMachPortInvalidate(sessionPrivate->callbackPort);
		CFRelease(sessionPrivate->callbackPort);
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	if (scd_status != SCD_OK) {
		return scd_status;
	}

	/* set notifier active */
	sessionPrivate->notifyStatus     = Using_NotifierInformViaCallback;
	sessionPrivate->callbackFunction = func;
	sessionPrivate->callbackArgument = arg;

	if (SCDOptionGet(session, kSCDOptionUseCFRunLoop)) {
		/* Creating/adding a run loop source for the port */
		sessionPrivate->callbackRunLoopSource =
			CFMachPortCreateRunLoopSource(NULL, sessionPrivate->callbackPort, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(),
				   sessionPrivate->callbackRunLoopSource,
				   kCFRunLoopDefaultMode);
	} else {
		pthread_attr_t		tattr;

		SCDLog(LOG_DEBUG, CFSTR("Starting background thread to watch for notifications..."));
		pthread_attr_init(&tattr);
		pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
		pthread_attr_setstacksize(&tattr, 96 * 1024); // each thread gets a 96K stack
		pthread_create(&sessionPrivate->callbackHelper,
			       &tattr,
			       watcherThread,
			       (void *)session);
		pthread_attr_destroy(&tattr);
		SCDLog(LOG_DEBUG, CFSTR("  thread id=0x%08x"), sessionPrivate->callbackHelper);
	}

	return SCD_OK;
}
