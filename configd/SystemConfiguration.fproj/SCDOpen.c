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
#include <servers/bootstrap.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


SCDSessionRef
_SCDSessionCreatePrivate()
{
	SCDSessionRef		newSession;
	SCDSessionPrivateRef	newPrivate;

	/* allocate space */
	newSession = (SCDSessionRef)CFAllocatorAllocate(NULL, sizeof(SCDSessionPrivate), 0);
	newPrivate = (SCDSessionPrivateRef)newSession;

	/* server side of the "configd" session */
	newPrivate->server         = MACH_PORT_NULL;

	/* per-session flags */
	SCDOptionSet(newSession, kSCDOptionDebug,        SCDOptionGet(NULL, kSCDOptionDebug       ));
	SCDOptionSet(newSession, kSCDOptionVerbose,      SCDOptionGet(NULL, kSCDOptionVerbose     ));
	SCDOptionSet(newSession, kSCDOptionIsLocked,     FALSE);
	SCDOptionSet(newSession, kSCDOptionUseSyslog,    SCDOptionGet(NULL, kSCDOptionUseSyslog   ));
	SCDOptionSet(newSession, kSCDOptionUseCFRunLoop, SCDOptionGet(NULL, kSCDOptionUseCFRunLoop));

	/* SCDKeys being watched */
	newPrivate->keys   = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	newPrivate->reKeys = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

	/* No notifications pending */
	newPrivate->notifyStatus		= NotifierNotRegistered;

	/* "client" information about active (notification) callback */
	newPrivate->callbackFunction		= NULL;
	newPrivate->callbackArgument		= NULL;
	newPrivate->callbackPort		= NULL;
	newPrivate->callbackRunLoopSource	= NULL;	/* XXX */
	newPrivate->callbackHelper		= NULL;

	/* "server" information associated with SCDNotifierInformViaMachPort(); */
	newPrivate->notifyPort			= MACH_PORT_NULL;
	newPrivate->notifyPortIdentifier	= 0;

	/* "server" information associated with SCDNotifierInformViaFD(); */
	newPrivate->notifyFile			= -1;
	newPrivate->notifyFileIdentifier	= 0;

	/* "server" information associated with SCDNotifierInformViaSignal(); */
	newPrivate->notifySignal		= 0;
	newPrivate->notifySignalTask		= TASK_NULL;

	return newSession;
}


SCDStatus
SCDOpen(SCDSessionRef *session, CFStringRef name)
{
	SCDSessionPrivateRef	sessionPrivate;
	kern_return_t		status;
	mach_port_t		bootstrap_port;
	mach_port_t		server;
	CFDataRef		xmlName;		/* serialized name */
	xmlData_t		myNameRef;
	CFIndex			myNameLen;
	SCDStatus		scd_status;

	SCDLog(LOG_DEBUG, CFSTR("SCDOpen:"));
	SCDLog(LOG_DEBUG, CFSTR("  name = %@"), name);

	/*
	 * allocate and initialize a new session
	 */
	sessionPrivate = (SCDSessionPrivateRef)_SCDSessionCreatePrivate();
	*session       = (SCDSessionRef)sessionPrivate;

	status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("task_get_bootstrap_port(): %s"), mach_error_string(status));
		CFAllocatorDeallocate(NULL, sessionPrivate);
		*session = NULL;
		return SCD_NOSERVER;
	}

	status = bootstrap_look_up(bootstrap_port, SCD_SERVER, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			CFAllocatorDeallocate(NULL, sessionPrivate);
			*session = NULL;
			return SCD_NOSERVER;
			break;
		default :
#ifdef	DEBUG
			SCDLog(LOG_DEBUG, CFSTR("bootstrap_status: %s"), mach_error_string(status));
#endif	/* DEBUG */
			CFAllocatorDeallocate(NULL, sessionPrivate);
			*session = NULL;
			return SCD_NOSERVER;
	}

	/* serialize the name */
	xmlName = CFPropertyListCreateXMLData(NULL, name);
	myNameRef = (xmlData_t)CFDataGetBytePtr(xmlName);
	myNameLen = CFDataGetLength(xmlName);

	/* open a new session with the server */
	status = configopen(server, myNameRef, myNameLen, &sessionPrivate->server, (int *)&scd_status);

	/* clean up */
	CFRelease(xmlName);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("configopen(): %s"), mach_error_string(status));
		CFAllocatorDeallocate(NULL, sessionPrivate);
		*session = NULL;
		return SCD_NOSERVER;
	}

	SCDLog(LOG_DEBUG, CFSTR("  server port = %d"), sessionPrivate->server);
	return scd_status;
}
