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

#include "configd.h"
#include "session.h"

SCDStatus
_SCDNotifierInformViaMachPort(SCDSessionRef session, mach_msg_id_t identifier, mach_port_t *port)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	CFStringRef		sessionKey;
	CFDictionaryRef		info;

	SCDLog(LOG_DEBUG, CFSTR("_SCDNotifierInformViaMachPort:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	if (sessionPrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return SCD_NOTIFIERACTIVE;
	}

	if (*port == MACH_PORT_NULL) {
		/* sorry, you must specify a valid mach port */
		return SCD_INVALIDARGUMENT;
	}

	/* push out a notification if any changes are pending */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);
	info = CFDictionaryGetValue(sessionData, sessionKey);
	CFRelease(sessionKey);
	if (info && CFDictionaryContainsKey(info, kSCDChangedKeys)) {
		CFNumberRef	sessionNum;

		if (needsNotification == NULL)
			needsNotification = CFSetCreateMutable(NULL,
							       0,
							       &kCFTypeSetCallBacks);

		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionPrivate->server);
		CFSetAddValue(needsNotification, sessionNum);
		CFRelease(sessionNum);
	}

	return SCD_OK;
}


kern_return_t
_notifyviaport(mach_port_t	server,
	       mach_port_t	port,
	       mach_msg_id_t	identifier,
	       int		*scd_status
)
{
	serverSessionRef	mySession = getSession(server);
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)mySession->session;

	SCDLog(LOG_DEBUG, CFSTR("Send mach message when a notification key changes."));
	SCDLog(LOG_DEBUG, CFSTR("  server     = %d"), server);
	SCDLog(LOG_DEBUG, CFSTR("  port       = %d"), port);
	SCDLog(LOG_DEBUG, CFSTR("  message id = %d"), identifier);

	if (sessionPrivate->notifyPort != MACH_PORT_NULL) {
		SCDLog(LOG_DEBUG, CFSTR("  destroying old callback mach port %d"), sessionPrivate->notifyPort);
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->notifyPort);
	}

	*scd_status = _SCDNotifierInformViaMachPort(mySession->session, identifier, &port);

	if (*scd_status == SCD_OK) {
		/* save notification port, requested identifier, and set notifier active */
		sessionPrivate->notifyStatus         = Using_NotifierInformViaMachPort;
		sessionPrivate->notifyPort           = port;
		sessionPrivate->notifyPortIdentifier = identifier;
	}

	return KERN_SUCCESS;
}
