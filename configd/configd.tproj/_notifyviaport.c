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
#include "session.h"

int
__SCDynamicStoreNotifyMachPort(SCDynamicStoreRef	store,
			       mach_msg_id_t		identifier,
			       mach_port_t		*port)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	CFStringRef			sessionKey;
	CFDictionaryRef			info;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreNotifyMachPort:"));

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;		/* you must have an open session to play */
	}

	if (storePrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return kSCStatusNotifierActive;
	}

	if (*port == MACH_PORT_NULL) {
		/* sorry, you must specify a valid mach port */
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
_notifyviaport(mach_port_t	server,
	       mach_port_t	port,
	       mach_msg_id_t	identifier,
	       int		*sc_status
)
{
	serverSessionRef		mySession = getSession(server);
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)mySession->store;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Send mach message when a notification key changes."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server     = %d"), server);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  port       = %d"), port);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  message id = %d"), identifier);

	if (storePrivate->notifyPort != MACH_PORT_NULL) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  destroying old callback mach port %d"), storePrivate->notifyPort);
		(void) mach_port_destroy(mach_task_self(), storePrivate->notifyPort);
	}

	*sc_status = __SCDynamicStoreNotifyMachPort(mySession->store, identifier, &port);

	if (*sc_status == kSCStatusOK) {
		/* save notification port, requested identifier, and set notifier active */
		storePrivate->notifyStatus         = Using_NotifierInformViaMachPort;
		storePrivate->notifyPort           = port;
		storePrivate->notifyPortIdentifier = identifier;
	}

	return KERN_SUCCESS;
}
