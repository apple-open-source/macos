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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "configd.h"
#include "session.h"


SCDStatus
_SCDNotifierInformViaFD(SCDSessionRef	session,
		       int32_t		identifier,
		       int		*fd)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	int			sock;
	CFStringRef		sessionKey;
	CFDictionaryRef		info;

	SCDLog(LOG_DEBUG, CFSTR("_SCDNotifierInformViaFD:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	if (sessionPrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return SCD_NOTIFIERACTIVE;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		SCDLog(LOG_NOTICE, CFSTR("socket: %s"), strerror(errno));
		return SCD_FAILED;
	}

	*fd = sock;

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
_notifyviafd(mach_port_t		server,
	     xmlData_t			pathRef,
	     mach_msg_type_number_t	pathLen,
	     int			identifier,
	     int			*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)mySession->session;
	struct sockaddr_un	un;
	int			sock;
	int			bufSiz  = sizeof(sessionPrivate->notifyFileIdentifier);
	int			nbioYes = 1;

	SCDLog(LOG_DEBUG, CFSTR("Send message via UNIX domain socket when a notification key changes."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);
	SCDLog(LOG_DEBUG, CFSTR("  path   = %s"), pathRef);

	/*
	 * if socket currently open, close it!
	 */
	/* validate the UNIX domain socket path */
	if (pathLen > (sizeof(un.sun_path) - 1)) {
		SCDLog(LOG_NOTICE, CFSTR("domain socket path length too long!"));
		status = vm_deallocate(mach_task_self(), (vm_address_t)pathRef, pathLen);
		if (status != KERN_SUCCESS) {
			SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	/* un-serialize the UNIX domain socket path */
	un.sun_family = AF_UNIX;
	bcopy(pathRef, un.sun_path, pathLen);
	un.sun_path[pathLen] = '\0';
	status = vm_deallocate(mach_task_self(), (vm_address_t)pathRef, pathLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}

	/* do common sanity checks, get socket */
	*scd_status = _SCDNotifierInformViaFD(mySession->session, identifier, &sock);

	/* check status of _SCDNotifierInformViaFD() */
	if (*scd_status != SCD_OK) {
		return KERN_SUCCESS;
	}

	/* establish the connection, get ready for a read() */
	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) == -1) {
		SCDLog(LOG_DEBUG, CFSTR("connect: %s"), strerror(errno));
		(void) close(sock);
		sessionPrivate->notifyStatus = NotifierNotRegistered;
		sessionPrivate->notifyFile   = -1;
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	SCDLog(LOG_NOTICE, CFSTR("  fd     = %d"), sock);
	(void) unlink(un.sun_path);

	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSiz, sizeof(bufSiz)) < 0) {
		SCDLog(LOG_DEBUG, CFSTR("setsockopt: %s"), strerror(errno));
		(void) close(sock);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	if (ioctl(sock, FIONBIO, &nbioYes) == -1) {
		SCDLog(LOG_DEBUG, CFSTR("ioctl(,FIONBIO,): %s"), strerror(errno));
		(void) close(sock);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	/* set notifier active */
	sessionPrivate->notifyStatus         = Using_NotifierInformViaFD;
	sessionPrivate->notifyFile           = sock;
	sessionPrivate->notifyFileIdentifier = identifier;

	return KERN_SUCCESS;
}
