/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * April 5, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "configd.h"
#include "session.h"


__private_extern__
int
__SCDynamicStoreNotifyFileDescriptor(SCDynamicStoreRef	store,
				     int32_t		identifier,
				     int		*fd)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sock;
	CFStringRef			sessionKey;
	CFDictionaryRef			info;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreNotifyFileDescriptor:"));

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (storePrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return kSCStatusNotifierActive;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		SCLog(_configd_verbose, LOG_NOTICE, CFSTR("socket: %s"), strerror(errno));
		return kSCStatusFailed;
	}

	*fd = sock;

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


__private_extern__
kern_return_t
_notifyviafd(mach_port_t		server,
	     xmlData_t			pathRef,
	     mach_msg_type_number_t	pathLen,
	     int			identifier,
	     int			*sc_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)mySession->store;
	struct sockaddr_un	un;
	int			sock;
	int			bufSiz  = sizeof(storePrivate->notifyFileIdentifier);
	int			nbioYes = 1;

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Send message via UNIX domain socket when a notification key changes."));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  server = %d"), server);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  path   = %s"), pathRef);
	}

	/*
	 * if socket currently open, close it!
	 */
	/* validate the UNIX domain socket path */
	if (pathLen > (sizeof(un.sun_path) - 1)) {
		SCLog(_configd_verbose, LOG_NOTICE, CFSTR("domain socket path length too long!"));
		status = vm_deallocate(mach_task_self(), (vm_address_t)pathRef, pathLen);
		if (status != KERN_SUCCESS) {
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	/* un-serialize the UNIX domain socket path */
	un.sun_family = AF_UNIX;
	bcopy(pathRef, un.sun_path, pathLen);
	un.sun_path[pathLen] = '\0';
	status = vm_deallocate(mach_task_self(), (vm_address_t)pathRef, pathLen);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}

	if (!mySession) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		return KERN_SUCCESS;
	}

	/* do common sanity checks, get socket */
	*sc_status = __SCDynamicStoreNotifyFileDescriptor(mySession->store, identifier, &sock);

	/* check status of __SCDynamicStoreNotifyFileDescriptor() */
	if (*sc_status != kSCStatusOK) {
		return KERN_SUCCESS;
	}

	/* establish the connection, get ready for a read() */
	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) == -1) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("connect: %s"), strerror(errno));
		(void) close(sock);
		storePrivate->notifyStatus = NotifierNotRegistered;
		storePrivate->notifyFile   = -1;
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	SCLog(_configd_verbose, LOG_NOTICE, CFSTR("  fd     = %d"), sock);
	(void) unlink(un.sun_path);

	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSiz, sizeof(bufSiz)) < 0) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("setsockopt: %s"), strerror(errno));
		(void) close(sock);
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	if (ioctl(sock, FIONBIO, &nbioYes) == -1) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("ioctl(,FIONBIO,): %s"), strerror(errno));
		(void) close(sock);
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	/* set notifier active */
	storePrivate->notifyStatus         = Using_NotifierInformViaFD;
	storePrivate->notifyFile           = sock;
	storePrivate->notifyFileIdentifier = identifier;

	return KERN_SUCCESS;
}
